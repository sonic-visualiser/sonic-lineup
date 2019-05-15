TEMPLATE = lib

exists(config.pri) {
    include(config.pri)
}

!exists(config.pri) {
    include(noconfig.pri)
}

CONFIG -= qt
CONFIG += plugin no_plugin_name_prefix release warn_on

TARGET = tuning-difference

OBJECTS_DIR = tuning-difference/o

INCLUDEPATH += $$PWD/vamp-plugin-sdk $$PWD/constant-q-cpp

QMAKE_CXXFLAGS -= -Werror

win32-msvc* {
    LIBS += -EXPORT:vampGetPluginDescriptor
}
win32-g++* {
    LIBS += -Wl,--version-script=$$PWD/tuning-difference/vamp-plugin.map
}
linux* {
    LIBS += -Wl,--version-script=$$PWD/tuning-difference/vamp-plugin.map
}
macx* {
    LIBS += -exported_symbols_list $$PWD/tuning-difference/vamp-plugin.list
}

SOURCES += \
    tuning-difference/src/TuningDifference.cpp
    tuning-difference/src/plugins.cpp

HEADERS += \
    tuning-difference/src/TuningDifference.h

