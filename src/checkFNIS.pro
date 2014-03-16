#-------------------------------------------------
#
# Project created by QtCreator 2013-10-07T21:11:42
#
#-------------------------------------------------

TARGET = checkFNIS
TEMPLATE = lib

CONFIG += plugins
CONFIG += dll

contains(QT_VERSION, "^5.*") {
	QT += widgets
}

DEFINES += CHECKFNIS_LIBRARY

SOURCES += checkfnis.cpp

HEADERS += checkfnis.h

include(../plugin_template.pri)

OTHER_FILES += \
    checkfnis.json
