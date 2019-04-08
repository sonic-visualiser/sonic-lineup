
TEMPLATE = app

exists(config.pri) {
    include(config.pri)
}

!exists(config.pri) {
    include(noconfig.pri)
}

include(base.pri)

QT += network xml gui widgets svg

TARGET = "Sonic Vector"
linux*:TARGET = sonic-vector
solaris*:TARGET = sonic-vector

!win32 {
    PRE_TARGETDEPS += $$PWD/libbase.a
    QMAKE_POST_LINK += cp checker/vamp-plugin-load-checker .
}

linux* {

    vect_bins.path = $$PREFIX_PATH/bin/
    vect_bins.files = checker/vamp-plugin-load-checker piper-vamp-simple-server sonic-vector
    vect_bins.CONFIG = no_check_exist

#    vect_desktop.path = $$PREFIX_PATH/share/applications/
#    vect_desktop.files = sonic-visualiser.desktop
#    vect_desktop.CONFIG = no_check_exist

#    vect_icon.path = $$PREFIX_PATH/share/icons/hicolor/scalable/apps/
#    vect_icon.files = icons/sonic-visualiser.svg
#    vect_icon.CONFIG = no_check_exist
    
     INSTALLS += vect_bins
#vect_desktop vect_icon
}

TRANSLATIONS += \
        i18n/sonic-visualiser_ru.ts \
	i18n/sonic-visualiser_en_GB.ts \
	i18n/sonic-visualiser_en_US.ts \
	i18n/sonic-visualiser_cs_CZ.ts

OBJECTS_DIR = o
MOC_DIR = o

#ICON = icons/sv-macicon.icns
#RC_FILE = icons/sv.rc

RESOURCES += vect.qrc

# Mac integration
QMAKE_INFO_PLIST = deploy/osx/Info.plist

include(svgui/files.pri)
include(svapp/files.pri)

for (file, SVGUI_SOURCES)    { SOURCES += $$sprintf("svgui/%1",    $$file) }
for (file, SVAPP_SOURCES)    { SOURCES += $$sprintf("svapp/%1",    $$file) }

for (file, SVGUI_HEADERS)    { HEADERS += $$sprintf("svgui/%1",    $$file) }
for (file, SVAPP_HEADERS)    { HEADERS += $$sprintf("svapp/%1",    $$file) }

HEADERS += \
        main/MainWindow.h \
        main/PreferencesDialog.h

SOURCES +=  \
	main/main.cpp \
        main/MainWindow.cpp \
        main/PreferencesDialog.cpp 

