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

INCLUDEPATH += $$PWD/vamp-plugin-sdk $$PWD/constant-q-cpp $$PWD/constant-q-cpp/cq $$PWD/constant-q-cpp/src/ext/kissfft $$PWD/constant-q-cpp/src/ext/kissfft/tools

QMAKE_CXXFLAGS -= -Werror

DEFINES += kiss_fft_scalar=double

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
    constant-q-cpp/src/CQKernel.cpp \
    constant-q-cpp/src/ConstantQ.cpp \
    constant-q-cpp/src/CQSpectrogram.cpp \
    constant-q-cpp/src/CQInverse.cpp \
    constant-q-cpp/src/Chromagram.cpp \
    constant-q-cpp/src/Pitch.cpp \
    constant-q-cpp/src/dsp/FFT.cpp \
    constant-q-cpp/src/dsp/KaiserWindow.cpp \
    constant-q-cpp/src/dsp/MathUtilities.cpp \
    constant-q-cpp/src/dsp/Resampler.cpp \
    constant-q-cpp/src/dsp/SincWindow.cpp \
    constant-q-cpp/src/ext/kissfft/kiss_fft.c \
    constant-q-cpp/src/ext/kissfft/tools/kiss_fftr.c \
    tuning-difference/src/TuningDifference.cpp \
    tuning-difference/src/plugins.cpp \
    vamp-plugin-sdk/src/vamp-sdk/PluginAdapter.cpp \
    vamp-plugin-sdk/src/vamp-sdk/RealTime.cpp

HEADERS += \
    tuning-difference/src/TuningDifference.h

