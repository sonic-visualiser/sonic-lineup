
TEMPLATE = lib

exists(config.pri) {
    include(config.pri)
}

!exists(config.pri) {
    include(noconfig.pri)
}

CONFIG -= qt
CONFIG += plugin no_plugin_name_prefix release warn_on

TARGET = qm-vamp-plugins

OBJECTS_DIR = qm-vamp-plugins/o

INCLUDEPATH += \
    $$PWD/vamp-plugin-sdk \
    $$PWD/qm-vamp-plugins/lib/qm-dsp \
    $$PWD/qm-vamp-plugins/lib/qm-dsp/ext/kissfft \
    $$PWD/qm-vamp-plugins/lib/qm-dsp/ext/kissfft/tools \
    $$PWD/qm-vamp-plugins/lib/qm-dsp/ext/clapack/include \
    $$PWD/qm-vamp-plugins/lib/qm-dsp/ext/cblas/include

QMAKE_CXXFLAGS -= -Werror

DEFINES += NO_BLAS_WRAP ADD_ kiss_fft_scalar=double 

win32-msvc* {
    LIBS += -EXPORT:vampGetPluginDescriptor
}
win32-g++* {
    LIBS += -Wl,--version-script=$$PWD/qm-vamp-plugins/vamp-plugin.map
}
linux* {
    DEFINES += USE_PTHREADS
    LIBS += -Wl,--version-script=$$PWD/qm-vamp-plugins/vamp-plugin.map
}
macx* {
    DEFINES += USE_PTHREADS
    LIBS += -exported_symbols_list $$PWD/qm-vamp-plugins/vamp-plugin.list
}

SOURCES += \
    qm-vamp-plugins/g2cstubs.c \
    qm-vamp-plugins/plugins/AdaptiveSpectrogram.cpp \
    qm-vamp-plugins/plugins/BarBeatTrack.cpp \
    qm-vamp-plugins/plugins/BeatTrack.cpp \
    qm-vamp-plugins/plugins/DWT.cpp \
    qm-vamp-plugins/plugins/OnsetDetect.cpp \
    qm-vamp-plugins/plugins/ChromagramPlugin.cpp \
    qm-vamp-plugins/plugins/ConstantQSpectrogram.cpp \
    qm-vamp-plugins/plugins/KeyDetect.cpp \
    qm-vamp-plugins/plugins/MFCCPlugin.cpp \
    qm-vamp-plugins/plugins/SegmenterPlugin.cpp \
    qm-vamp-plugins/plugins/SimilarityPlugin.cpp \
    qm-vamp-plugins/plugins/TonalChangeDetect.cpp \
    qm-vamp-plugins/plugins/Transcription.cpp \
    qm-vamp-plugins/libmain.cpp \
    qm-vamp-plugins/lib/qm-dsp/base/Pitch.cpp \
    qm-vamp-plugins/lib/qm-dsp/base/KaiserWindow.cpp \
    qm-vamp-plugins/lib/qm-dsp/base/SincWindow.cpp \
    qm-vamp-plugins/lib/qm-dsp/dsp/chromagram/Chromagram.cpp \
    qm-vamp-plugins/lib/qm-dsp/dsp/chromagram/ConstantQ.cpp \
    qm-vamp-plugins/lib/qm-dsp/dsp/keydetection/GetKeyMode.cpp \
    qm-vamp-plugins/lib/qm-dsp/dsp/mfcc/MFCC.cpp \
    qm-vamp-plugins/lib/qm-dsp/dsp/onsets/DetectionFunction.cpp \
    qm-vamp-plugins/lib/qm-dsp/dsp/onsets/PeakPicking.cpp \
    qm-vamp-plugins/lib/qm-dsp/dsp/phasevocoder/PhaseVocoder.cpp \
    qm-vamp-plugins/lib/qm-dsp/dsp/rateconversion/Decimator.cpp \
    qm-vamp-plugins/lib/qm-dsp/dsp/rateconversion/DecimatorB.cpp \
    qm-vamp-plugins/lib/qm-dsp/dsp/rateconversion/Resampler.cpp \
    qm-vamp-plugins/lib/qm-dsp/dsp/rhythm/BeatSpectrum.cpp \
    qm-vamp-plugins/lib/qm-dsp/dsp/segmentation/cluster_melt.c \
    qm-vamp-plugins/lib/qm-dsp/dsp/segmentation/ClusterMeltSegmenter.cpp \
    qm-vamp-plugins/lib/qm-dsp/dsp/segmentation/cluster_segmenter.c \
    qm-vamp-plugins/lib/qm-dsp/dsp/segmentation/Segmenter.cpp \
    qm-vamp-plugins/lib/qm-dsp/dsp/signalconditioning/DFProcess.cpp \
    qm-vamp-plugins/lib/qm-dsp/dsp/signalconditioning/Filter.cpp \
    qm-vamp-plugins/lib/qm-dsp/dsp/signalconditioning/FiltFilt.cpp \
    qm-vamp-plugins/lib/qm-dsp/dsp/signalconditioning/Framer.cpp \
    qm-vamp-plugins/lib/qm-dsp/dsp/tempotracking/DownBeat.cpp \
    qm-vamp-plugins/lib/qm-dsp/dsp/tempotracking/TempoTrack.cpp \
    qm-vamp-plugins/lib/qm-dsp/dsp/tempotracking/TempoTrackV2.cpp \
    qm-vamp-plugins/lib/qm-dsp/dsp/tonal/ChangeDetectionFunction.cpp \
    qm-vamp-plugins/lib/qm-dsp/dsp/tonal/TCSgram.cpp \
    qm-vamp-plugins/lib/qm-dsp/dsp/tonal/TonalEstimator.cpp \
    qm-vamp-plugins/lib/qm-dsp/dsp/transforms/DCT.cpp \
    qm-vamp-plugins/lib/qm-dsp/dsp/transforms/FFT.cpp \
    qm-vamp-plugins/lib/qm-dsp/dsp/wavelet/Wavelet.cpp \
    qm-vamp-plugins/lib/qm-dsp/hmm/hmm.c \
    qm-vamp-plugins/lib/qm-dsp/maths/Correlation.cpp \
    qm-vamp-plugins/lib/qm-dsp/maths/CosineDistance.cpp \
    qm-vamp-plugins/lib/qm-dsp/maths/KLDivergence.cpp \
    qm-vamp-plugins/lib/qm-dsp/maths/MathUtilities.cpp \
    qm-vamp-plugins/lib/qm-dsp/maths/pca/pca.c \
    qm-vamp-plugins/lib/qm-dsp/thread/Thread.cpp \
    qm-vamp-plugins/lib/qm-dsp/ext/kissfft/kiss_fft.c \
    qm-vamp-plugins/lib/qm-dsp/ext/kissfft/tools/kiss_fftr.c \
    qm-vamp-plugins/lib/qm-dsp/ext/clapack/src/dgetrf.c \
    qm-vamp-plugins/lib/qm-dsp/ext/clapack/src/dgetri.c \
    qm-vamp-plugins/lib/qm-dsp/ext/clapack/src/dgetf2.c \
    qm-vamp-plugins/lib/qm-dsp/ext/clapack/src/xerbla.c \
    qm-vamp-plugins/lib/qm-dsp/ext/clapack/src/dlaswp.c \
    qm-vamp-plugins/lib/qm-dsp/ext/clapack/src/dtrtri.c \
    qm-vamp-plugins/lib/qm-dsp/ext/clapack/src/ilaenv.c \
    qm-vamp-plugins/lib/qm-dsp/ext/clapack/src/iparmq.c \
    qm-vamp-plugins/lib/qm-dsp/ext/clapack/src/s_cat.c \
    qm-vamp-plugins/lib/qm-dsp/ext/clapack/src/s_copy.c \
    qm-vamp-plugins/lib/qm-dsp/ext/clapack/src/s_cmp.c \
    qm-vamp-plugins/lib/qm-dsp/ext/clapack/src/pow_di.c \
    qm-vamp-plugins/lib/qm-dsp/ext/clapack/src/ieeeck.c \
    qm-vamp-plugins/lib/qm-dsp/ext/clapack/src/i_nint.c \
    qm-vamp-plugins/lib/qm-dsp/ext/clapack/src/dtrti2.c \
    qm-vamp-plugins/lib/qm-dsp/ext/clapack/src/f77_aloc.c \
    qm-vamp-plugins/lib/qm-dsp/ext/clapack/src/exit_.c \
    qm-vamp-plugins/lib/qm-dsp/ext/cblas/src/dgemm.c \
    qm-vamp-plugins/lib/qm-dsp/ext/cblas/src/ddot.c \
    qm-vamp-plugins/lib/qm-dsp/ext/cblas/src/dgemv.c \
    qm-vamp-plugins/lib/qm-dsp/ext/cblas/src/dswap.c \
    qm-vamp-plugins/lib/qm-dsp/ext/cblas/src/dtrsm.c \
    qm-vamp-plugins/lib/qm-dsp/ext/cblas/src/dger.c \
    qm-vamp-plugins/lib/qm-dsp/ext/cblas/src/idamax.c \
    qm-vamp-plugins/lib/qm-dsp/ext/cblas/src/dscal.c \
    qm-vamp-plugins/lib/qm-dsp/ext/cblas/src/dtrmm.c \
    qm-vamp-plugins/lib/qm-dsp/ext/cblas/src/lsame.c \
    qm-vamp-plugins/lib/qm-dsp/ext/cblas/src/dlamch.c \
    qm-vamp-plugins/lib/qm-dsp/ext/cblas/src/dtrmv.c \
    qm-vamp-plugins/lib/qm-dsp/ext/cblas/src/cblas_globals.c \
    qm-vamp-plugins/lib/qm-dsp/ext/cblas/src/cblas_dgemm.c \
    qm-vamp-plugins/lib/qm-dsp/ext/cblas/src/cblas_ddot.c \
    qm-vamp-plugins/lib/qm-dsp/ext/cblas/src/cblas_xerbla.c \
    vamp-plugin-sdk/src/vamp-sdk/PluginAdapter.cpp \
    vamp-plugin-sdk/src/vamp-sdk/RealTime.cpp

HEADERS += \
    qm-vamp-plugins/plugins/AdaptiveSpectrogram.h \
    qm-vamp-plugins/plugins/BarBeatTrack.h \
    qm-vamp-plugins/plugins/BeatTrack.h \
    qm-vamp-plugins/plugins/DWT.h \
    qm-vamp-plugins/plugins/OnsetDetect.h \
    qm-vamp-plugins/plugins/ChromagramPlugin.h \
    qm-vamp-plugins/plugins/ConstantQSpectrogram.h \
    qm-vamp-plugins/plugins/KeyDetect.h \
    qm-vamp-plugins/plugins/MFCCPlugin.h \
    qm-vamp-plugins/plugins/SegmenterPlugin.h \
    qm-vamp-plugins/plugins/SimilarityPlugin.h \
    qm-vamp-plugins/plugins/TonalChangeDetect.h \
    qm-vamp-plugins/plugins/Transcription.h \
    qm-vamp-plugins/lib/qm-dsp/base/Pitch.h \
    qm-vamp-plugins/lib/qm-dsp/base/Window.h \
    qm-vamp-plugins/lib/qm-dsp/base/KaiserWindow.h \
    qm-vamp-plugins/lib/qm-dsp/base/SincWindow.h \
    qm-vamp-plugins/lib/qm-dsp/dsp/chromagram/Chromagram.h \
    qm-vamp-plugins/lib/qm-dsp/dsp/chromagram/ConstantQ.h \
    qm-vamp-plugins/lib/qm-dsp/dsp/keydetection/GetKeyMode.h \
    qm-vamp-plugins/lib/qm-dsp/dsp/mfcc/MFCC.h \
    qm-vamp-plugins/lib/qm-dsp/dsp/onsets/DetectionFunction.h \
    qm-vamp-plugins/lib/qm-dsp/dsp/onsets/PeakPicking.h \
    qm-vamp-plugins/lib/qm-dsp/dsp/phasevocoder/PhaseVocoder.h \
    qm-vamp-plugins/lib/qm-dsp/dsp/rateconversion/Decimator.h \
    qm-vamp-plugins/lib/qm-dsp/dsp/rateconversion/DecimatorB.h \
    qm-vamp-plugins/lib/qm-dsp/dsp/rateconversion/Resampler.h \
    qm-vamp-plugins/lib/qm-dsp/dsp/rhythm/BeatSpectrum.h \
    qm-vamp-plugins/lib/qm-dsp/dsp/segmentation/cluster_melt.h \
    qm-vamp-plugins/lib/qm-dsp/dsp/segmentation/ClusterMeltSegmenter.h \
    qm-vamp-plugins/lib/qm-dsp/dsp/segmentation/cluster_segmenter.h \
    qm-vamp-plugins/lib/qm-dsp/dsp/segmentation/Segmenter.h \
    qm-vamp-plugins/lib/qm-dsp/dsp/segmentation/segment.h \
    qm-vamp-plugins/lib/qm-dsp/dsp/signalconditioning/DFProcess.h \
    qm-vamp-plugins/lib/qm-dsp/dsp/signalconditioning/Filter.h \
    qm-vamp-plugins/lib/qm-dsp/dsp/signalconditioning/FiltFilt.h \
    qm-vamp-plugins/lib/qm-dsp/dsp/signalconditioning/Framer.h \
    qm-vamp-plugins/lib/qm-dsp/dsp/tempotracking/DownBeat.h \
    qm-vamp-plugins/lib/qm-dsp/dsp/tempotracking/TempoTrack.h \
    qm-vamp-plugins/lib/qm-dsp/dsp/tempotracking/TempoTrackV2.h \
    qm-vamp-plugins/lib/qm-dsp/dsp/tonal/ChangeDetectionFunction.h \
    qm-vamp-plugins/lib/qm-dsp/dsp/tonal/TCSgram.h \
    qm-vamp-plugins/lib/qm-dsp/dsp/tonal/TonalEstimator.h \
    qm-vamp-plugins/lib/qm-dsp/dsp/transforms/DCT.h \
    qm-vamp-plugins/lib/qm-dsp/dsp/transforms/FFT.h \
    qm-vamp-plugins/lib/qm-dsp/dsp/wavelet/Wavelet.h \
    qm-vamp-plugins/lib/qm-dsp/hmm/hmm.h \
    qm-vamp-plugins/lib/qm-dsp/maths/Correlation.h \
    qm-vamp-plugins/lib/qm-dsp/maths/CosineDistance.h \
    qm-vamp-plugins/lib/qm-dsp/maths/KLDivergence.h \
    qm-vamp-plugins/lib/qm-dsp/maths/MathAliases.h \
    qm-vamp-plugins/lib/qm-dsp/maths/MathUtilities.h \
    qm-vamp-plugins/lib/qm-dsp/maths/MedianFilter.h \
    qm-vamp-plugins/lib/qm-dsp/maths/Polyfit.h \
    qm-vamp-plugins/lib/qm-dsp/maths/pca/pca.h \
    qm-vamp-plugins/lib/qm-dsp/thread/AsynchronousTask.h \
    qm-vamp-plugins/lib/qm-dsp/thread/BlockAllocator.h \
    qm-vamp-plugins/lib/qm-dsp/thread/Thread.h \
    qm-vamp-plugins/lib/qm-dsp/ext/kissfft/kiss_fft.h \
    qm-vamp-plugins/lib/qm-dsp/ext/kissfft/tools/kiss_fftr.h
