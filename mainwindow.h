#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "sms_sender.h"

#include <QMainWindow>

#ifndef Q_MOC_RUN
#include <boost/thread.hpp>
#endif // Q_MOC_RUN

#include <map>
#include <string>

namespace Ui
{
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = 0);
    ~MainWindow();

private slots:
    void on_add_thread_button_clicked();
    void on_remove_thread_button_clicked();
    void on_use_this_phone_number_button_clicked();

private:
    void init_ui();
    void cleanup_ui();
    void init_sms_sender();
    void cleanup_sms_sender();
    void start_threads_checker();
    void wait_for_threads_checker();
    void show_message_box(const QString& message);
    void threads_checker();
    bool verify_phone_number(const std::string& phone);

    Ui::MainWindow* ui;
    boost::thread _threads_checker_th;
    std::map<std::string, std::string> _threads_content;
    boost::mutex _threads_content_sync;
    SmsSender _sms_sender;
    QStringList _to_phone_numbers_list;
    boost::mutex _to_phone_numbers_list_sync;
};

#endif // MAINWINDOW_H
