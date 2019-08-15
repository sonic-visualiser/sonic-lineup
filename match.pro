
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

INCLUDEPATH += $$PWD/vamp-plugin-sdk $$PWD/tipic/src $$PWD/tipic/qm-dsp $$PWD/tipic/qm-dsp/ext/kissfft $$PWD/tipic/qm-dsp/ext/kissfft/tools

QMAKE_CXXFLAGS -= -Werror

QMAKE_CFLAGS += -Dkiss_fft_scalar=double
QMAKE_CXXFLAGS += -Dkiss_fft_scalar=double

#DEFINES += USE_COMPACT_TYPES
DEFINES += USE_PRECISE_TYPES

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
    match/src/Matcher.cpp \
    match/src/MatchFeatureFeeder.cpp \
    match/src/MatchPipeline.cpp \
    match/src/MatchVampPlugin.cpp \
    match/src/MatchTipicVampPlugin.cpp \
    match/src/Path.cpp \
    tipic/src/PitchFilterbank.cpp \
    tipic/src/CRP.cpp \
    tipic/src/Chroma.cpp \
    tipic/src/FeatureDownsample.cpp \
    tipic/src/CENS.cpp \
    tipic/qm-dsp/dsp/signalconditioning/Filter.cpp \
    tipic/qm-dsp/dsp/transforms/DCT.cpp \
    tipic/qm-dsp/dsp/transforms/FFT.cpp \
    tipic/qm-dsp/dsp/rateconversion/Resampler.cpp \
    tipic/qm-dsp/maths/MathUtilities.cpp \
    tipic/qm-dsp/base/KaiserWindow.cpp \
    tipic/qm-dsp/base/SincWindow.cpp \
    tipic/qm-dsp/ext/kissfft/kiss_fft.c \
    tipic/qm-dsp/ext/kissfft/tools/kiss_fftr.c \
    vamp-plugin-sdk/src/vamp-sdk/PluginAdapter.cpp \
    vamp-plugin-sdk/src/vamp-sdk/RealTime.cpp \
    match/src/libmain.cpp

HEADERS += \
    match/src/DistanceMetric.h \
    match/src/FeatureConditioner.h \
    match/src/FeatureExtractor.h \
    match/src/Finder.h \
    match/src/Matcher.h \
    match/src/MatchFeatureFeeder.h \
    match/src/MatchPipeline.h \
    match/src/MatchTypes.h \
    match/src/MatchVampPlugin.h \
    match/src/MatchTipicVampPlugin.h \
    match/src/Path.h


