#include "mainwindow.h"

#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

#include <QApplication>

void init_logger()
{
    boost::log::add_file_log
    (
        boost::log::keywords::file_name = "rsdn_watcher_log_%Y-%m-%d_%H-%M-%S.%N.log ",
        boost::log::keywords::rotation_size = 10 * 1024 * 1024,
        boost::log::keywords::time_based_rotation = boost::log::sinks::file::rotation_at_time_point(0, 0, 0),
        boost::log::keywords::format = "[%TimeStamp%]: %Message%"
    );
    boost::log::add_common_attributes();
}

int main(int argc, char* argv[])
{
    init_logger();

    QApplication a(argc, argv);

    try
    {
        MainWindow w;
        w.show();
        return a.exec();
    }
    catch (const std::exception&) {}

    return a.exec();
}
