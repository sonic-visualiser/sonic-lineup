
TEMPLATE = app

SV_UNIT_PACKAGES = vamp vamp-hostsdk fftw3f samplerate jack portaudio mad id3tag oggz fishsound lrdf raptor sndfile liblo
load(../sonic-visualiser/sv.prf)

CONFIG += sv qt thread warn_on stl rtti exceptions
QT += xml network

TARGET = vect

ICON = icons/sv-macicon.icns

DEPENDPATH += . ../sonic-visualiser audioio document i18n main osc transform
INCLUDEPATH += . ../sonic-visualiser audioio document transform osc main
LIBPATH = ../sonic-visualiser/view ../sonic-visualiser/layer ../sonic-visualiser/data ../sonic-visualiser/widgets ../sonic-visualiser/plugin ../sonic-visualiser/base ../sonic-visualiser/system $$LIBPATH

contains(DEFINES, BUILD_STATIC):LIBS -= -ljack

LIBS = -lsvview -lsvlayer -lsvdata -lsvwidgets -lsvplugin -lsvbase -lsvsystem $$LIBS

PRE_TARGETDEPS += ../sonic-visualiser/view/libsvview.a \
                  ../sonic-visualiser/layer/libsvlayer.a \
                  ../sonic-visualiser/data/libsvdata.a \
                  ../sonic-visualiser/widgets/libsvwidgets.a \
                  ../sonic-visualiser/plugin/libsvplugin.a \
                  ../sonic-visualiser/base/libsvbase.a \
                  ../sonic-visualiser/system/libsvsystem.a

OBJECTS_DIR = tmp_obj
MOC_DIR = tmp_moc

# Input
HEADERS += audioio/AudioCallbackPlaySource.h \
           audioio/AudioCallbackPlayTarget.h \
           audioio/AudioCoreAudioTarget.h \
           audioio/AudioGenerator.h \
           audioio/AudioJACKTarget.h \
           audioio/AudioPortAudioTarget.h \
           audioio/AudioTargetFactory.h \
           audioio/PhaseVocoderTimeStretcher.h \
           audioio/PlaySpeedRangeMapper.h \
           document/Document.h \
           document/SVFileReader.h \
           main/MainWindow.h \
           main/PreferencesDialog.h \
           osc/OSCMessage.h \
           osc/OSCQueue.h \
           transform/FeatureExtractionPluginTransform.h \
           transform/PluginTransform.h \
           transform/RealTimePluginTransform.h \
           transform/Transform.h \
           transform/TransformFactory.h
SOURCES += audioio/AudioCallbackPlaySource.cpp \
           audioio/AudioCallbackPlayTarget.cpp \
           audioio/AudioCoreAudioTarget.cpp \
           audioio/AudioGenerator.cpp \
           audioio/AudioJACKTarget.cpp \
           audioio/AudioPortAudioTarget.cpp \
           audioio/AudioTargetFactory.cpp \
           audioio/PhaseVocoderTimeStretcher.cpp \
           audioio/PlaySpeedRangeMapper.cpp \
           document/Document.cpp \
           document/SVFileReader.cpp \
           main/main.cpp \
           main/MainWindow.cpp \
           main/PreferencesDialog.cpp \
           osc/OSCMessage.cpp \
           osc/OSCQueue.cpp \
           transform/FeatureExtractionPluginTransform.cpp \
           transform/PluginTransform.cpp \
           transform/RealTimePluginTransform.cpp \
           transform/Transform.cpp \
           transform/TransformFactory.cpp
RESOURCES += vect.qrc


