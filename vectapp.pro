
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
}

linux* {

    vect_bins.path = $$PREFIX_PATH/bin/
    vect_bins.files = sonic-lineup
    vect_bins.CONFIG = no_check_exist executable

    vect_support.path = $$PREFIX_PATH/lib/sonic-lineup/
    vect_support.files = checker/vamp-plugin-load-checker azi.so match-vamp-plugin.so nnls-chroma.so pyin.so qm-vamp-plugins.so tuning-difference.so
    vect_support.CONFIG = no_check_exist executable

    vect_desktop.path = $$PREFIX_PATH/share/applications/
    vect_desktop.files = sonic-lineup.desktop
    vect_desktop.CONFIG = no_check_exist

    vect_icon.path = $$PREFIX_PATH/share/icons/hicolor/scalable/apps/
    vect_icon.files = icons/sonic-lineup-icon.svg
    vect_icon.CONFIG = no_check_exist
    
    INSTALLS += vect_bins vect_support vect_desktop vect_icon
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
        main/IntroDialog.h \
        main/MainWindow.h \
        main/NetworkPermissionTester.h \
        main/PreferencesDialog.h \
        main/SmallSession.h

SOURCES +=  \
        main/IntroDialog.cpp \
	main/main.cpp \
        main/MainWindow.cpp \
        main/NetworkPermissionTester.cpp \
        main/PreferencesDialog.cpp \
        main/SmallSession.cpp

win32-msvc* {
    LIBS += -los
}

macx* {
    QMAKE_POST_LINK += cp checker/vamp-plugin-load-checker . && deploy/osx/deploy.sh $$shell_quote(Sonic Lineup)
}

linux {
    QMAKE_POST_LINK += cp checker/vamp-plugin-load-checker .
}

