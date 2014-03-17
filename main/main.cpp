/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Vect
    An experimental audio player for plural recordings of a work
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2012 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "MainWindow.h"

#include "system/System.h"
#include "system/Init.h"
#include "base/TempDirectory.h"
#include "base/PropertyContainer.h"
#include "base/Preferences.h"
#include "widgets/TipDialog.h"

#include <QMetaType>
#include <QApplication>
#include <QDesktopWidget>
#include <QMessageBox>
#include <QTranslator>
#include <QLocale>
#include <QSettings>
#include <QIcon>
#include <QSessionManager>
#include <QDir>

#include <iostream>
#include <signal.h>

static QMutex cleanupMutex;
static bool cleanedUp = false;

static void
signalHandler(int /* signal */)
{
    // Avoid this happening more than once across threads

    std::cerr << "signalHandler: cleaning up and exiting" << std::endl;
    cleanupMutex.lock();
    if (!cleanedUp) {
        TempDirectory::getInstance()->cleanup();
        cleanedUp = true;
    }
    cleanupMutex.unlock();
    exit(0);
}

class VectApplication : public QApplication
{
public:
    VectApplication(int argc, char **argv) :
        QApplication(argc, argv),
        m_mainWindow(0) { }
    virtual ~VectApplication() { }

    void setMainWindow(MainWindow *mw) { m_mainWindow = mw; }
    void releaseMainWindow() { m_mainWindow = 0; }

    virtual void commitData(QSessionManager &manager) {
        if (!m_mainWindow) return;
        bool mayAskUser = manager.allowsInteraction();
        bool success = m_mainWindow->commitData(mayAskUser);
        manager.release();
        if (!success) manager.cancel();
    }

protected:
    MainWindow *m_mainWindow;
};

int
main(int argc, char **argv)
{
    StoreStartupLocale();

    VectApplication application(argc, argv);

    QStringList args = application.arguments();

    signal(SIGINT,  signalHandler);
    signal(SIGTERM, signalHandler);

#ifndef Q_WS_WIN32
    signal(SIGHUP,  signalHandler);
    signal(SIGQUIT, signalHandler);
#endif

    svSystemSpecificInitialisation();

    bool audioOutput = true;

    if (args.contains("--help") || args.contains("-h") || args.contains("-?")) {
        std::cerr << QApplication::tr(
            "\nSonic Vector is a comparative viewer for sets of related audio recordings.\n\nUsage:\n\n  %1 [--no-audio] [<file1>, <file2>...]\n\n  --no-audio: Do not attempt to open an audio output device\n  <file1>, <file2>...: Audio files; Sonic Vector is designed for comparative\nviewing of multiple recordings of the same music or other related material.\n").arg(argv[0]).toStdString() << std::endl;
        exit(2);
    }

    if (args.contains("--no-audio")) audioOutput = false;

    QApplication::setOrganizationName("sonic-visualiser");
    QApplication::setOrganizationDomain("sonicvisualiser.org");
    QApplication::setApplicationName("Sonic Vector");

    QIcon icon;
    int sizes[] = { 16, 22, 24, 32, 48, 64, 128 };
    for (int i = 0; i < (int)(sizeof(sizes)/sizeof(sizes[0])); ++i) {
        icon.addFile(QString(":icons/sv-%1x%2.png").arg(sizes[i]).arg(sizes[i]));
    }
    QApplication::setWindowIcon(icon);

    QString language = QLocale::system().name();

    QTranslator qtTranslator;
    QString qtTrName = QString("qt_%1").arg(language);
    std::cerr << "Loading " << qtTrName.toStdString() << "..." << std::endl;
    bool success = false;
    if (!(success = qtTranslator.load(qtTrName))) {
        QString qtDir = getenv("QTDIR");
        if (qtDir != "") {
            success = qtTranslator.load
                (qtTrName, QDir(qtDir).filePath("translations"));
        }
    }
    if (!success) {
        std::cerr << "Failed to load Qt translation for locale" << std::endl;
    }
    application.installTranslator(&qtTranslator);

    //!!! load sv translations, plus vect translations
    QTranslator svTranslator;
    QString svTrName = QString("sonic-vector_%1").arg(language);
    std::cerr << "Loading " << svTrName.toStdString() << "..." << std::endl;
    svTranslator.load(svTrName, ":i18n");
    application.installTranslator(&svTranslator);

    // Permit size_t and PropertyName to be used as args in queued signal calls
    qRegisterMetaType<size_t>("size_t");
    qRegisterMetaType<PropertyContainer::PropertyName>("PropertyContainer::PropertyName");

    MainWindow *gui = new MainWindow(audioOutput);
    application.setMainWindow(gui);

    QDesktopWidget *desktop = QApplication::desktop();
    QRect available = desktop->availableGeometry();

    int width = available.width() * 2 / 3;
    int height = available.height() / 2;
    if (height < 450) height = available.height() * 2 / 3;
    if (width > height * 2) width = height * 2;

    QSettings settings;
    settings.beginGroup("MainWindow");

    QSize size = settings.value("size", QSize(width, height)).toSize();
    gui->resizeConstrained(size);

    if (settings.contains("position")) {
        QRect prevrect(settings.value("position").toPoint(), size);
        if (!(available & prevrect).isEmpty()) {
            gui->move(prevrect.topLeft());
        }
    }

    if (settings.value("maximised", false).toBool()) {
        gui->setWindowState(Qt::WindowMaximized);
    }

    settings.endGroup();
    
    gui->show();

    bool haveSession = false;
    bool haveMainModel = false;
    bool havePriorCommandLineModel = false;

    for (QStringList::iterator i = args.begin(); i != args.end(); ++i) {

        MainWindow::FileOpenStatus status = MainWindow::FileOpenFailed;

        if (i == args.begin()) continue;
        if (i->startsWith('-')) continue;

        if (i->startsWith("http:") || i->startsWith("ftp:")) {
            std::cerr << "opening URL: \"" << i->toStdString() << "\"..." << std::endl;
            status = gui->open(*i);
            continue;
        }

        QString path = *i;

        if (path.endsWith("sv")) {
            if (!haveSession) {
                status = gui->openSessionFile(path);
                if (status == MainWindow::FileOpenSucceeded) {
                    haveSession = true;
                    haveMainModel = true;
                }
            } else {
                std::cerr << "WARNING: Ignoring additional session file argument \"" << path.toStdString() << "\"" << std::endl;
                status = MainWindow::FileOpenSucceeded;
            }
        }
        if (status != MainWindow::FileOpenSucceeded) {
            if (!haveMainModel) {
                status = gui->open(path, MainWindow::ReplaceMainModel);
                if (status == MainWindow::FileOpenSucceeded) {
                    haveMainModel = true;
                }
            } else {
                if (haveSession && !havePriorCommandLineModel) {
                    status = gui->open(path, MainWindow::AskUser);
                    if (status == MainWindow::FileOpenSucceeded) {
                        havePriorCommandLineModel = true;
                    }
                } else {
                    status = gui->open(path, MainWindow::CreateAdditionalModel);
                }
            }
        }
        if (status == MainWindow::FileOpenFailed) {
	    QMessageBox::critical
                (gui, QMessageBox::tr("Failed to open file"),
                 QMessageBox::tr("File \"%1\" could not be opened").arg(path));
        }
    }

    int rv = application.exec();
//    std::cerr << "application.exec() returned " << rv << std::endl;

    cleanupMutex.lock();
    TempDirectory::getInstance()->cleanup();
    application.releaseMainWindow();

    delete gui;

    cleanupMutex.unlock();

    return rv;
}
