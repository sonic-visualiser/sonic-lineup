
TEMPLATE = lib

exists(config.pri) {
    include(config.pri)
}

!exists(config.pri) {
    include(noconfig.pri)
}

CONFIG -= qt
CONFIG += plugin no_plugin_name_prefix release warn_on

TARGET = match-vamp-plugin

OBJECTS_DIR = match/o

INCLUDEPATH += $$PWD/vamp-plugin-sdk

QMAKE_CXXFLAGS -= -Werror

DEFINES += USE_COMPACT_TYPES

win32-msvc* {
    LIBS += -EXPORT:vampGetPluginDescriptor
}
win32-g++* {
    LIBS += -Wl,--version-script=$$PWD/match/vamp-plugin.map
}
linux* {
    LIBS += -Wl,--version-script=$$PWD/match/vamp-plugin.map
}
macx* {
    LIBS += -exported_symbols_list $$PWD/match/vamp-plugin.list
}

SOURCES += \
    match/src/DistanceMetric.cpp \
    match/src/FeatureConditioner.cpp \
    match/src/FeatureExtractor.cpp \
    match/src/Finder.cpp \
    match/src/FullDTW.cpp \
    match/src/Matcher.cpp \
    match/src/MatchFeatureFeeder.cpp \
    match/src/MatchPipeline.cpp \
    match/src/MatchVampPlugin.cpp \
    match/src/Path.cpp \
    match/src/SubsequenceMatchVampPlugin.cpp \
    match/src/libmain.cpp \
    vamp-plugin-sdk/src/vamp-sdk/FFT.cpp \
    vamp-plugin-sdk/src/vamp-sdk/PluginAdapter.cpp \
    vamp-plugin-sdk/src/vamp-sdk/RealTime.cpp

HEADERS += \
    match/src/DistanceMetric.h \
    match/src/FeatureConditioner.h \
    match/src/FeatureExtractor.h \
    match/src/Finder.h \
    match/src/FullDTW.h \
    match/src/Matcher.h \
    match/src/MatchFeatureFeeder.h \
    match/src/MatchPipeline.h \
    match/src/MatchTypes.h \
    match/src/MatchVampPlugin.h \
    match/src/Path.h \
    match/src/SubsequenceMatchVampPlugin.h


