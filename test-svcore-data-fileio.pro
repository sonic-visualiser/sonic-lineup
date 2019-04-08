
TEMPLATE = app

exists(config.pri) {
    include(config.pri)
}

!exists(config.pri) {
    include(noconfig.pri)
}

include(base.pri)

CONFIG += console
QT += network xml testlib
QT -= gui

win32-x-g++:QMAKE_LFLAGS += -Wl,-subsystem,console
macx*: CONFIG -= app_bundle

TARGET = test-svcore-data-fileio

OBJECTS_DIR = o
MOC_DIR = o

include(svcore/data/fileio/test/files.pri)

for (file, TEST_SOURCES) { SOURCES += $$sprintf("svcore/data/fileio/test/%1", $$file) }
for (file, TEST_HEADERS) { HEADERS += $$sprintf("svcore/data/fileio/test/%1", $$file) }

!win32* {
    QMAKE_POST_LINK = ./$${TARGET}
}
