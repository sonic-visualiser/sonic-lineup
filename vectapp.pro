
TEMPLATE = app

include(config.pri)

CONFIG += qt thread warn_on stl rtti exceptions
QT += network xml gui

TARGET = Vect
linux*:TARGET = vect
solaris*:TARGET = vect

DEPENDPATH += . svcore svgui svapp
INCLUDEPATH += . svcore svgui svapp

OBJECTS_DIR = o
MOC_DIR = o

contains(DEFINES, BUILD_STATIC):LIBS -= -ljack

LIBS = -Lsvapp -Lsvgui -Lsvcore -lsvapp -lsvgui -lsvcore $$LIBS

win* {
PRE_TARGETDEPS += svapp/svapp.lib \
                  svgui/svgui.lib \
                  svcore/svcore.lib
}
!win* {
PRE_TARGETDEPS += svapp/libsvapp.a \
                  svgui/libsvgui.a \
                  svcore/libsvcore.a
}

RESOURCES += vect.qrc

HEADERS += main/MainWindow.h \
           main/PreferencesDialog.h
SOURCES += main/main.cpp \
           main/MainWindow.cpp \
           main/PreferencesDialog.cpp

QMAKE_INFO_PLIST = deploy/osx/Info.plist



