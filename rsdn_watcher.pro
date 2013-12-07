#-------------------------------------------------
#
# Project created by QtCreator 2013-11-29T17:13:37
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = rsdn_watcher
TEMPLATE = app

LIBS += -Le:/libs/boost_1_54_0/stage/lib
LIBS += e:/libs/curl-7.29.0/lib/DLL-Release/libcurl_imp.lib
INCLUDEPATH += e:/libs/boost_1_54_0 e:/libs/curl-7.29.0/include

SOURCES += main.cpp\
        mainwindow.cpp \
    sms_sender.cpp

HEADERS  += mainwindow.h \
    http.h \
    sms_sender.h \
    auxiliary.h

FORMS    += mainwindow.ui
