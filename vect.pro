
TEMPLATE = app

SV_UNIT_PACKAGES = vamp vamp-hostsdk fftw3f samplerate jack portaudio mad id3tag oggz fishsound lrdf raptor sndfile liblo
load(../sonic-visualiser/sv.prf)

CONFIG += sv qt thread warn_on stl rtti exceptions
QT += xml network

TARGET = vect

ICON = icons/sv-macicon.icns

DEPENDPATH += . ../sonic-visualiser i18n main
INCLUDEPATH += . ../sonic-visualiser main
LIBPATH = ../sonic-visualiser/framework ../sonic-visualiser/audioio ../sonic-visualiser/view ../sonic-visualiser/layer ../sonic-visualiser/data ../sonic-visualiser/widgets ../sonic-visualiser/plugin ../sonic-visualiser/base ../sonic-visualiser/system $$LIBPATH

contains(DEFINES, BUILD_STATIC):LIBS -= -ljack

LIBS = -lsvframework -lsvaudioio -lsvview -lsvlayer -lsvdata -lsvwidgets -lsvplugin -lsvbase -lsvsystem $$LIBS

PRE_TARGETDEPS += ../sonic-visualiser/framework/libsvframework.a \
                  ../sonic-visualiser/audioio/libsvaudioio.a \
                  ../sonic-visualiser/view/libsvview.a \
                  ../sonic-visualiser/layer/libsvlayer.a \
                  ../sonic-visualiser/data/libsvdata.a \
                  ../sonic-visualiser/widgets/libsvwidgets.a \
                  ../sonic-visualiser/plugin/libsvplugin.a \
                  ../sonic-visualiser/base/libsvbase.a \
                  ../sonic-visualiser/system/libsvsystem.a

OBJECTS_DIR = tmp_obj
MOC_DIR = tmp_moc

# Input
HEADERS += main/MainWindow.h \
           main/PreferencesDialog.h
SOURCES += main/main.cpp \
           main/MainWindow.cpp \
           main/PreferencesDialog.cpp

RESOURCES += vect.qrc


