#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "auxiliary.h"
#include "http.h"

#include <boost/algorithm/string/trim.hpp>
#include <boost/log/trivial.hpp>

#include <QDebug>
#include <QInputDialog>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QString>

#include <ctime>

MainWindow::MainWindow(QWidget* parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    init_ui();
    init_sms_sender();
    start_threads_checker();
}

MainWindow::~MainWindow()
{
    wait_for_threads_checker();
    cleanup_sms_sender();
    cleanup_ui();
}

void MainWindow::init_ui()
{
    BOOST_LOG_TRIVIAL(info) << "Trying to initialize UI...";
    ui->setupUi(this);
    BOOST_LOG_TRIVIAL(info) << "Success";
}

void MainWindow::cleanup_ui()
{
    BOOST_LOG_TRIVIAL(info) << "Trying to deinitialize UI...";
    delete ui;
    BOOST_LOG_TRIVIAL(info) << "Success";
}

void MainWindow::init_sms_sender()
{
    BOOST_LOG_TRIVIAL(info) << "Trying to initialize SMS sender...";
    try
    {
        _sms_sender.init();
    }
    catch (const SmsSenderException& ex)
    {
        BOOST_LOG_TRIVIAL(error) << "An error occurred while initializing SMS sender: " << ex.what();
        throw;
    }
    _sms_sender.set_twilio_account_sid("AC88bc6b479b76079e081a6634005cde9e");
    _sms_sender.set_twilio_account_token("7f5f1e7a5540add4b3d0d8b9014019f6");
    _sms_sender.set_twilio_from_phone("(612)260-9615");
    BOOST_LOG_TRIVIAL(info) << "Success";
}

void MainWindow::cleanup_sms_sender()
{
    BOOST_LOG_TRIVIAL(info) << "Trying to deinitialize SMS sender...";
    _sms_sender.cleanup();
    BOOST_LOG_TRIVIAL(info) << "Success";
}

void MainWindow::start_threads_checker()
{
    BOOST_LOG_TRIVIAL(info) << "Trying to start threads checker...";
    try
    {
        _threads_checker_th = boost::thread(&MainWindow::threads_checker, this);
    }
    catch (const boost::thread_resource_error& ex)
    {
        BOOST_LOG_TRIVIAL(error) << "An error occurred while starting threads checker: " << ex.what();
        throw;
    }
    BOOST_LOG_TRIVIAL(info) << "Success";
}

void MainWindow::wait_for_threads_checker()
{
    BOOST_LOG_TRIVIAL(info) << "Waiting for the threads checker...";
    _threads_checker_th.interrupt();
    try
    {
        _threads_checker_th.join();
    }
    catch (const std::exception&) {}
    BOOST_LOG_TRIVIAL(info) << "Done";
}

void MainWindow::show_message_box(const QString& message)
{
    QMessageBox msg_box;
    msg_box.setText(message);
    msg_box.exec();
}

void MainWindow::threads_checker()
{
    while (true)
    {
        for (int cur_row = 0; cur_row < ui->threads_list_widget->count(); ++cur_row)
        {
            QListWidgetItem* item = ui->threads_list_widget->item(cur_row);
            QString thread_link = item->text().remove("http://");

            int request_uri_begin = thread_link.indexOf('/');
            QString host(thread_link.left(request_uri_begin));
            QString request_uri(thread_link.mid(request_uri_begin));
            std::string server_response_body;
            try
            {
                const http::response server_response = http::get_request(
                    host.toStdString()
                    , request_uri.toStdString()
                );
                server_response_body = server_response.message_body;
            }
            catch (const http::exception& ex)
            {
                BOOST_LOG_TRIVIAL(error) << "An error occurred while doing GET request (URL - " << thread_link.toStdString() << "): " << ex.what();
                continue;
            }

            const std::string& thread_link_str = thread_link.toStdString();

            boost::lock_guard<boost::mutex> lock(_threads_content_sync);
            const auto& itr = _threads_content.find(thread_link_str);
            if (itr != _threads_content.end())
            {
                if (itr->second != server_response_body)
                {
                    std::string title = get_string_between(server_response_body, "<title>", "</title>");
                    boost::trim(title);

                    boost::lock_guard<boost::mutex> lock(_to_phone_numbers_list_sync);
                    foreach (QString cur_phone, _to_phone_numbers_list)
                    {
                        if (!_sms_sender.send_sms(
                                cur_phone.toStdString()
                                , "New answer in the thread \"" + title + '\"'
                            )
                        )
                        {
                            BOOST_LOG_TRIVIAL(error) << "Unable to send SMS";
                            continue;
                        }
                    }
                }
            }

            _threads_content[thread_link_str] = server_response_body;
        }

        boost::this_thread::sleep_for(boost::chrono::seconds(5));
    }
}

void MainWindow::on_add_thread_button_clicked()
{
    QString thread_link = QInputDialog::getText(
        this
        , "Add thread to watch"
        , "Input thread link: "
    );
    if (thread_link.isEmpty())
    {
        return;
    }

    QRegExp reg_exp("(http://)?(www\\.)?rsdn.ru/forum/[^/]*/[^/]*.flat");
    if (!reg_exp.exactMatch(thread_link))
    {
        show_message_box("Invalid URL");
        return;
    }

    QListWidgetItem* item = new QListWidgetItem(thread_link);
    ui->threads_list_widget->addItem(item);
}

void MainWindow::on_remove_thread_button_clicked()
{
    int cur_row = ui->threads_list_widget->currentRow();
    QListWidgetItem* item = ui->threads_list_widget->takeItem(cur_row);

    boost::lock_guard<boost::mutex> lock(_threads_content_sync);
    _threads_content.erase(item->text().toStdString());
}

bool MainWindow::verify_phone_number(const std::string& phone)
{
    std::srand(std::time(0));
    QString num_to_verify = QString::number(std::rand() % 1000);

    if (!_sms_sender.send_sms(phone, num_to_verify.toStdString()))
    {
        show_message_box("Unable to send activation SMS. Please try again later");
        return false;
    }

    QString input = QInputDialog::getText(
        this
        , "Activation number"
        , ("Enter activation number for " + phone + ": ").c_str()
    );
    if (input.isEmpty())
    {
        return false;
    }

    if (num_to_verify != input)
    {
        show_message_box("Wrong verification code");
        return false;
    }

    show_message_box("Accept");
    return true;
}

void MainWindow::on_use_this_phone_number_button_clicked()
{
    QString to_phone_numbers = ui->your_phone_number_line_edit->text();
    if (to_phone_numbers.isEmpty())
    {
        show_message_box("You need to enter dest. phone first");
        return;
    }

    QStringList to_phone_numbers_list = to_phone_numbers.split(QRegExp("[,;]"), QString::SkipEmptyParts);
    foreach (QString cur_phone, to_phone_numbers_list)
    {
        qDebug() << "begin" << cur_phone << "end";
        if (!verify_phone_number(cur_phone.toStdString()))
        {
            return;
        }
    }

    boost::lock_guard<boost::mutex> lock(_to_phone_numbers_list_sync);
    _to_phone_numbers_list = to_phone_numbers_list;
}
