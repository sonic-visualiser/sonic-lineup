
TEMPLATE = app

exists(config.pri) {
    include(config.pri)
}

!exists(config.pri) {
    include(noconfig.pri)
}

include(base.pri)

QT += network xml gui widgets svg

TARGET = "Sonic Lineup"
linux*:TARGET = sonic-lineup
solaris*:TARGET = sonic-lineup

!win32 {
    PRE_TARGETDEPS += $$PWD/libbase.a
    QMAKE_POST_LINK += cp checker/vamp-plugin-load-checker .
}

linux* {

    vect_bins.path = $$PREFIX_PATH/bin/
    vect_bins.files = checker/vamp-plugin-load-checker piper-vamp-simple-server sonic-lineup
    vect_bins.CONFIG = no_check_exist

#    vect_desktop.path = $$PREFIX_PATH/share/applications/
#    vect_desktop.files = sonic-visualiser.desktop
#    vect_desktop.CONFIG = no_check_exist

    vect_icon.path = $$PREFIX_PATH/share/icons/hicolor/scalable/apps/
    vect_icon.files = icons/sonic-lineup-icon.svg
    vect_icon.CONFIG = no_check_exist
    
     INSTALLS += vect_bins
#vect_desktop vect_icon
}

TRANSLATIONS += \
        i18n/vect_ru.ts \
	i18n/vect_en_GB.ts \
	i18n/vect_en_US.ts \
	i18n/vect_cs_CZ.ts

OBJECTS_DIR = o
MOC_DIR = o

ICON = icons/sonic-lineup-icon.icns
RC_FILE = icons/sonic-lineup.rc

RESOURCES += sonic-lineup.qrc

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
        main/NetworkPermissionTester.h \
        main/PreferencesDialog.h \
        main/SmallSession.h

SOURCES +=  \
	main/main.cpp \
        main/MainWindow.cpp \
        main/NetworkPermissionTester.cpp \
        main/PreferencesDialog.cpp \
        main/SmallSession.cpp

