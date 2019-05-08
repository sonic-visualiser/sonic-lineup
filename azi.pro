
TEMPLATE = lib

exists(config.pri) {
    include(config.pri)
}

!exists(config.pri) {
    include(noconfig.pri)
}

CONFIG -= qt
CONFIG += plugin no_plugin_name_prefix release warn_on

TARGET = azi

OBJECTS_DIR = azi/o

INCLUDEPATH += $$PWD/vamp-plugin-sdk

QMAKE_CXXFLAGS -= -Werror

win32-msvc* {
    LIBS += -EXPORT:vampGetPluginDescriptor
}
win32-g++* {
    LIBS += -Wl,--version-script=$$PWD/azi/vamp-plugin.map
}
linux* {
    LIBS += -Wl,--version-script=$$PWD/azi/vamp-plugin.map
}
macx* {
    LIBS += -exported_symbols_list $$PWD/azi/vamp-plugin.list
}

SOURCES += \
    azi/Azi.cpp \
    azi/plugins.cpp \
    vamp-plugin-sdk/src/vamp-sdk/FFT.cpp \
    vamp-plugin-sdk/src/vamp-sdk/PluginAdapter.cpp \
    vamp-plugin-sdk/src/vamp-sdk/RealTime.cpp

HEADERS += \
    azi/Azi.h

