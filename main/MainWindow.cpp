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

#include "../version.h"

#include "MainWindow.h"
#include "framework/Document.h"
#include "PreferencesDialog.h"

#include "view/Pane.h"
#include "view/PaneStack.h"
#include "data/model/WaveFileModel.h"
#include "data/model/SparseOneDimensionalModel.h"
#include "data/model/AlignmentModel.h"
#include "data/model/SparseOneDimensionalModel.h"
#include "base/StorageAdviser.h"
#include "view/ViewManager.h"
#include "base/Preferences.h"
#include "layer/WaveformLayer.h"
#include "layer/TimeRulerLayer.h"
#include "layer/TimeInstantLayer.h"
#include "layer/TimeValueLayer.h"
#include "layer/Colour3DPlotLayer.h"
#include "layer/SliceLayer.h"
#include "layer/SliceableLayer.h"
#include "view/Overview.h"
#include "widgets/PropertyBox.h"
#include "widgets/PropertyStack.h"
#include "widgets/AudioDial.h"
#include "widgets/LevelPanWidget.h"
#include "widgets/LevelPanToolButton.h"
#include "widgets/IconLoader.h"
#include "widgets/LayerTree.h"
#include "widgets/ListInputDialog.h"
#include "widgets/SubdividingMenu.h"
#include "widgets/NotifyingPushButton.h"
#include "widgets/KeyReference.h"
#include "audio/AudioCallbackPlaySource.h"
#include "audio/PlaySpeedRangeMapper.h"
#include "data/fileio/DataFileReaderFactory.h"
#include "data/fileio/PlaylistFileReader.h"
#include "data/fileio/WavFileWriter.h"
#include "data/fileio/CSVFileWriter.h"
#include "data/fileio/BZipFileDevice.h"
#include "data/fileio/FileSource.h"
#include "base/RecentFiles.h"
#include "transform/TransformFactory.h"
#include "transform/ModelTransformerFactory.h"
#include "base/PlayParameterRepository.h"
#include "base/XmlExportable.h"
#include "widgets/CommandHistory.h"
#include "base/Profiler.h"
#include "base/Clipboard.h"
#include "base/UnitDatabase.h"
#include "layer/ColourDatabase.h"
#include "data/osc/OSCQueue.h"

//!!!
#include "data/model/AggregateWaveModel.h"

// For version information
#include "vamp/vamp.h"
#include "vamp-sdk/PluginBase.h"
#include "plugin/api/ladspa.h"
#include "plugin/api/dssi.h"

#include <bqaudioio/SystemPlaybackTarget.h>
#include <bqaudioio/SystemAudioIO.h>

#include <QApplication>
#include <QMessageBox>
#include <QGridLayout>
#include <QLabel>
#include <QAction>
#include <QMenuBar>
#include <QToolBar>
#include <QToolButton>
#include <QButtonGroup>
#include <QInputDialog>
#include <QStatusBar>
#include <QTreeView>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QProcess>
#include <QShortcut>
#include <QSettings>
#include <QDateTime>
#include <QProcess>
#include <QCheckBox>
#include <QRegExp>
#include <QScrollArea>
#include <QCloseEvent>

#include <iostream>
#include <cstdio>
#include <errno.h>

using std::cerr;
using std::endl;

using std::vector;
using std::map;
using std::set;


MainWindow::MainWindow(bool withAudioOutput) :
    MainWindowBase(withAudioOutput ? WithAudioOutput : WithNothing),
    m_overview(0),
    m_mainMenusCreated(false),
    m_playbackMenu(0),
    m_recentFilesMenu(0),
    m_rightButtonMenu(0),
    m_rightButtonPlaybackMenu(0),
    m_deleteSelectedAction(0),
    m_ffwdAction(0),
    m_rwdAction(0),
    m_exiting(false),
    m_preferencesDialog(0),
    m_layerTreeView(0),
    m_keyReference(new KeyReference()),
    m_displayMode(WaveformMode),
    m_salientCalculating(false)
{
    setWindowTitle(tr("Sonic Vector"));

    StorageAdviser::setFixedRecommendation
        (StorageAdviser::Recommendation(StorageAdviser::UseDisc |
                                        StorageAdviser::ConserveSpace));

    UnitDatabase *udb = UnitDatabase::getInstance();
    udb->registerUnit("Hz");
    udb->registerUnit("dB");
    udb->registerUnit("s");

    ColourDatabase *cdb = ColourDatabase::getInstance();
    cdb->addColour(Qt::black, tr("Black"));
    cdb->addColour(Qt::darkRed, tr("Red"));
    cdb->addColour(Qt::darkBlue, tr("Blue"));
    cdb->addColour(Qt::darkGreen, tr("Green"));
    cdb->addColour(QColor(200, 50, 255), tr("Purple"));
    cdb->addColour(QColor(255, 150, 50), tr("Orange"));
    cdb->setUseDarkBackground(cdb->addColour(Qt::white, tr("White")), true);
    cdb->setUseDarkBackground(cdb->addColour(Qt::red, tr("Bright Red")), true);
    cdb->setUseDarkBackground(cdb->addColour(QColor(30, 150, 255), tr("Bright Blue")), true);
    cdb->setUseDarkBackground(cdb->addColour(Qt::green, tr("Bright Green")), true);
    cdb->setUseDarkBackground(cdb->addColour(QColor(225, 74, 255), tr("Bright Purple")), true);
    cdb->setUseDarkBackground(cdb->addColour(QColor(255, 188, 80), tr("Bright Orange")), true);

    Preferences::getInstance()->setResampleOnLoad(true);


    Preferences::getInstance()->setSpectrogramSmoothing
        (Preferences::SpectrogramInterpolated);
//        (Preferences::NoSpectrogramSmoothing);

    Preferences::getInstance()->setSpectrogramXSmoothing
        (Preferences::SpectrogramXInterpolated);
//        (Preferences::NoSpectrogramXSmoothing);

    QSettings settings;

    settings.beginGroup("LayerDefaults");

    settings.setValue("waveform",
                      QString("<layer scale=\"%1\" channelMode=\"%2\"/>")
                      .arg(int(WaveformLayer::MeterScale))
//                      .arg(int(WaveformLayer::LinearScale))
                      .arg(int(WaveformLayer::MergeChannels)));

    settings.setValue("timevalues",
                      QString("<layer plotStyle=\"%1\"/>")
                      .arg(int(TimeValueLayer::PlotStems)));

    settings.setValue("spectrogram",
                      QString("<layer channel=\"-1\" windowSize=\"1024\" windowHopLevel=\"2\"/>"));

    settings.setValue("melodicrange",
                      QString("<layer channel=\"-1\" gain=\"150\" normalizeVisibleArea=\"false\" normalizeColumns=\"false\" normalizeHybrid=\"true\" minFrequency=\"100\" maxFrequency=\"1200\" windowSize=\"4096\" windowOverlap=\"75\" binDisplay=\"0\" />"));

    settings.endGroup();

    settings.beginGroup("MainWindow");
    settings.setValue("showstatusbar", false);
    settings.endGroup();

    m_viewManager->setAlignMode(true);
    m_viewManager->setPlaySoloMode(true);
    m_viewManager->setToolMode(ViewManager::NavigateMode);
    m_viewManager->setZoomWheelsEnabled(false);
    m_viewManager->setIlluminateLocalFeatures(false);
    m_viewManager->setShowWorkTitle(true);

    loadStyle();
    
    QFrame *frame = new QFrame;
    setCentralWidget(frame);

    QGridLayout *layout = new QGridLayout;
    
    m_mainScroll = new QScrollArea(frame);
    m_mainScroll->setWidgetResizable(true);
    m_mainScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_mainScroll->setFrameShape(QFrame::NoFrame);

    m_paneStack->setLayoutStyle(PaneStack::NoPropertyStacks);
    m_paneStack->setShowAlignmentViews(true);
    m_mainScroll->setWidget(m_paneStack);
    
    QButtonGroup *bg = new QButtonGroup;
    IconLoader il;

    QFrame *buttonFrame = new QFrame;
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->setSpacing(0);
    buttonLayout->setMargin(0);
    buttonFrame->setLayout(buttonLayout);

    QToolButton *button = new QToolButton;
    button->setIcon(il.load("waveform"));
    button->setToolTip(tr("Waveform"));
    button->setCheckable(true);
    button->setChecked(true);
    button->setAutoRaise(true);
    bg->addButton(button);
    buttonLayout->addWidget(button);
    connect(button, SIGNAL(clicked()), this, SLOT(waveformModeSelected()));

    button = new QToolButton;
    button->setIcon(il.load("values"));
    button->setToolTip(tr("Novelty Curve"));
    button->setCheckable(true);
    button->setChecked(false);
    button->setAutoRaise(true);
    bg->addButton(button);
    buttonLayout->addWidget(button);
    connect(button, SIGNAL(clicked()), this, SLOT(curveModeSelected()));

    button = new QToolButton;
    button->setIcon(il.load("spectrogram"));
    button->setToolTip(tr("Full-Range Spectrogram"));
    button->setCheckable(true);
    button->setChecked(false);
    button->setAutoRaise(true);
    bg->addButton(button);
    buttonLayout->addWidget(button);
    connect(button, SIGNAL(clicked()), this, SLOT(spectrogramModeSelected()));

    button = new QToolButton;
    button->setIcon(il.load("melodogram"));
    button->setToolTip(tr("Melodic-Range Spectrogram"));
    button->setCheckable(true);
    button->setChecked(false);
    button->setAutoRaise(true);
    bg->addButton(button);
    buttonLayout->addWidget(button);
    connect(button, SIGNAL(clicked()), this, SLOT(melodogramModeSelected()));

    layout->addWidget(buttonFrame, 1, 0);

    m_overview = new Overview(frame);
    m_overview->setViewManager(m_viewManager);
    int overviewHeight = m_viewManager->scalePixelSize(35);
    if (overviewHeight < 40) overviewHeight = 40;
    m_overview->setFixedHeight(overviewHeight);
#ifndef _WIN32
    // For some reason, the contents of the overview never appear if we
    // make this setting on Windows.  I have no inclination at the moment
    // to track down the reason why.
    m_overview->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
#endif
    connect(m_overview, SIGNAL(contextHelpChanged(const QString &)),
            this, SLOT(contextHelpChanged(const QString &)));
    m_overview->hide();

    m_panLayer = new WaveformLayer;
    m_panLayer->setChannelMode(WaveformLayer::MergeChannels);
    m_panLayer->setAggressiveCacheing(true);
    m_overview->addLayer(m_panLayer);

    if (m_viewManager->getGlobalDarkBackground()) {
        m_panLayer->setBaseColour
            (ColourDatabase::getInstance()->getColourIndex(tr("Bright Green")));
    } else {
        m_panLayer->setBaseColour
            (ColourDatabase::getInstance()->getColourIndex(tr("Green")));
    }        

    m_playSpeed = new AudioDial(frame);
    m_playSpeed->setMinimum(0);
    m_playSpeed->setMaximum(120);
    m_playSpeed->setValue(60);
    m_playSpeed->setFixedWidth(overviewHeight);
    m_playSpeed->setFixedHeight(overviewHeight);
    m_playSpeed->setNotchesVisible(true);
    m_playSpeed->setPageStep(10);
    m_playSpeed->setObjectName(tr("Playback Speed"));
    m_playSpeed->setRangeMapper(new PlaySpeedRangeMapper);
    m_playSpeed->setDefaultValue(60);
    m_playSpeed->setShowToolTip(true);
    connect(m_playSpeed, SIGNAL(valueChanged(int)),
	    this, SLOT(playSpeedChanged(int)));
    connect(m_playSpeed, SIGNAL(mouseEntered()), this, SLOT(mouseEnteredWidget()));
    connect(m_playSpeed, SIGNAL(mouseLeft()), this, SLOT(mouseLeftWidget()));

    m_mainLevelPan = new LevelPanToolButton(frame);
    connect(m_mainLevelPan, SIGNAL(mouseEntered()), this, SLOT(mouseEnteredWidget()));
    connect(m_mainLevelPan, SIGNAL(mouseLeft()), this, SLOT(mouseLeftWidget()));
    m_mainLevelPan->setFixedHeight(overviewHeight);
    m_mainLevelPan->setFixedWidth(overviewHeight);
    m_mainLevelPan->setImageSize((overviewHeight * 3) / 4);
    m_mainLevelPan->setBigImageSize(overviewHeight * 3);

    m_playControlsSpacer = new QFrame;

    layout->setSpacing(m_viewManager->scalePixelSize(4));
    layout->addWidget(m_mainScroll, 0, 0, 1, 6);
    layout->addWidget(m_overview, 1, 1);
    layout->addWidget(m_playSpeed, 1, 2);
    layout->addWidget(m_playControlsSpacer, 1, 3);
    layout->addWidget(m_mainLevelPan, 1, 4);

    m_playControlsSpacer->setFixedSize(QSize(2, 2));
    layout->setColumnStretch(1, 10);
    
    frame->setLayout(layout);

    setupMenus();
    setupToolbars();
    setupHelpMenu();

    statusBar();

    setIconsVisibleInMenus(false);
    finaliseMenus();

    newSession();
}

MainWindow::~MainWindow()
{
    delete m_keyReference;
    delete m_preferencesDialog;
    delete m_layerTreeView;
    Profiles::getInstance()->dump();
}

void
MainWindow::setupMenus()
{
    if (!m_mainMenusCreated) {

#ifdef Q_OS_LINUX
        // In Ubuntu 14.04 the window's menu bar goes missing entirely
        // if the user is running any desktop environment other than Unity
        // (in which the faux single-menubar appears). The user has a
        // workaround, to remove the appmenu-qt5 package, but that is
        // awkward and the problem is so severe that it merits disabling
        // the system menubar integration altogether. Like this:
	menuBar()->setNativeMenuBar(false);
#endif

        m_rightButtonMenu = new QMenu();

        // No -- we don't want tear-off enabled on the right-button
        // menu.  If it is enabled, then simply right-clicking and
        // releasing will pop up the menu, activate the tear-off, and
        // leave the torn-off menu window in front of the main window.
        // That isn't desirable.  I'm not sure it ever would be, in a
        // context menu -- perhaps technically a Qt bug?
//        m_rightButtonMenu->setTearOffEnabled(true);
    }

    if (!m_mainMenusCreated) {
        CommandHistory::getInstance()->registerMenu(m_rightButtonMenu);
        m_rightButtonMenu->addSeparator();
    }

    setupFileMenu();
//    setupEditMenu();
    setupViewMenu();

    m_mainMenusCreated = true;
}

void
MainWindow::goFullScreen()
{
    if (!m_viewManager) return;

    if (m_viewManager->getZoomWheelsEnabled()) {
        // The wheels seem to end up in the wrong place in full-screen mode
        toggleZoomWheels();
    }

    QWidget *ps = m_mainScroll->takeWidget();
    ps->setParent(0);

    QShortcut *sc;

    sc = new QShortcut(QKeySequence("Esc"), ps);
    connect(sc, SIGNAL(activated()), this, SLOT(endFullScreen()));

    sc = new QShortcut(QKeySequence("F11"), ps);
    connect(sc, SIGNAL(activated()), this, SLOT(endFullScreen()));

    QAction *acts[] = {
        m_playAction, m_zoomInAction, m_zoomOutAction, m_zoomFitAction,
        m_scrollLeftAction, m_scrollRightAction, m_showPropertyBoxesAction
    };

    for (int i = 0; i < int(sizeof(acts)/sizeof(acts[0])); ++i) {
        sc = new QShortcut(acts[i]->shortcut(), ps);
        connect(sc, SIGNAL(activated()), acts[i], SLOT(trigger()));
    }

    ps->showFullScreen();
}

void
MainWindow::endFullScreen()
{
    // these were only created in goFullScreen:
    QObjectList cl = m_paneStack->children();
    foreach (QObject *o, cl) {
        QShortcut *sc = qobject_cast<QShortcut *>(o);
        if (sc) delete sc;
    }

    m_paneStack->showNormal();
    m_mainScroll->setWidget(m_paneStack);
}

void
MainWindow::setupFileMenu()
{
    if (m_mainMenusCreated) return;

    QMenu *menu = menuBar()->addMenu(tr("&File"));
    menu->setTearOffEnabled(false);
    QToolBar *toolbar = addToolBar(tr("File Toolbar"));

    m_keyReference->setCategory(tr("File and Session Management"));

    IconLoader il;

    QIcon icon = il.load("filenew");
    QAction *action = new QAction(icon, tr("&Clear Session"), this);
    action->setShortcut(tr("Ctrl+N"));
    action->setStatusTip(tr("Abandon the current session and start a new one"));
    connect(action, SIGNAL(triggered()), this, SLOT(newSession()));
    m_keyReference->registerShortcut(action);
    menu->addAction(action);
    toolbar->addAction(action);

    icon = il.load("fileopen");
    action = new QAction(icon, tr("&Add File..."), this);
    action->setShortcut(tr("Ctrl+O"));
    action->setStatusTip(tr("Add a file"));
    connect(action, SIGNAL(triggered()), this, SLOT(openFile()));
    m_keyReference->registerShortcut(action);
    menu->addAction(action);
    toolbar->addAction(action);

    action = new QAction(tr("Add Lo&cation..."), this);
    action->setShortcut(tr("Ctrl+Shift+O"));
    action->setStatusTip(tr("Add a file from a remote URL"));
    connect(action, SIGNAL(triggered()), this, SLOT(openLocation()));
    m_keyReference->registerShortcut(action);
    menu->addAction(action);

    menu->addSeparator();

    m_recentFilesMenu = menu->addMenu(tr("&Recent Locations"));
    m_recentFilesMenu->setTearOffEnabled(false);
    setupRecentFilesMenu();
    connect(&m_recentFiles, SIGNAL(recentChanged()),
            this, SLOT(setupRecentFilesMenu()));

    menu->addSeparator();
    action = new QAction(tr("&Preferences..."), this);
    action->setStatusTip(tr("Adjust the application preferences"));
    connect(action, SIGNAL(triggered()), this, SLOT(preferences()));
    menu->addAction(action);

    menu->addSeparator();
    action = new QAction(il.load("exit"), tr("&Quit"), this);
    action->setShortcut(tr("Ctrl+Q"));
    action->setStatusTip(tr("Exit Sonic Vector"));
    connect(action, SIGNAL(triggered()), this, SLOT(close()));
    m_keyReference->registerShortcut(action);
    menu->addAction(action);
}

void
MainWindow::setupEditMenu()
{
    if (m_mainMenusCreated) return;

    QMenu *menu = menuBar()->addMenu(tr("&Edit"));
    menu->setTearOffEnabled(false);
    CommandHistory::getInstance()->registerMenu(menu);
}

void
MainWindow::setupViewMenu()
{
    if (m_mainMenusCreated) return;

    IconLoader il;

    QAction *action = 0;

    m_keyReference->setCategory(tr("Panning and Navigation"));

    QMenu *menu = menuBar()->addMenu(tr("&View"));
    menu->setTearOffEnabled(false);
    m_scrollLeftAction = new QAction(tr("Scroll &Left"), this);
    m_scrollLeftAction->setShortcut(tr("Left"));
    m_scrollLeftAction->setStatusTip(tr("Scroll the current pane to the left"));
    connect(m_scrollLeftAction, SIGNAL(triggered()), this, SLOT(scrollLeft()));
    connect(this, SIGNAL(canScroll(bool)), m_scrollLeftAction, SLOT(setEnabled(bool)));
    m_keyReference->registerShortcut(m_scrollLeftAction);
    menu->addAction(m_scrollLeftAction);
	
    m_scrollRightAction = new QAction(tr("Scroll &Right"), this);
    m_scrollRightAction->setShortcut(tr("Right"));
    m_scrollRightAction->setStatusTip(tr("Scroll the current pane to the right"));
    connect(m_scrollRightAction, SIGNAL(triggered()), this, SLOT(scrollRight()));
    connect(this, SIGNAL(canScroll(bool)), m_scrollRightAction, SLOT(setEnabled(bool)));
    m_keyReference->registerShortcut(m_scrollRightAction);
    menu->addAction(m_scrollRightAction);
	
    action = new QAction(tr("&Jump Left"), this);
    action->setShortcut(tr("Ctrl+Left"));
    action->setStatusTip(tr("Scroll the current pane a big step to the left"));
    connect(action, SIGNAL(triggered()), this, SLOT(jumpLeft()));
    connect(this, SIGNAL(canScroll(bool)), action, SLOT(setEnabled(bool)));
    m_keyReference->registerShortcut(action);
    menu->addAction(action);
	
    action = new QAction(tr("J&ump Right"), this);
    action->setShortcut(tr("Ctrl+Right"));
    action->setStatusTip(tr("Scroll the current pane a big step to the right"));
    connect(action, SIGNAL(triggered()), this, SLOT(jumpRight()));
    connect(this, SIGNAL(canScroll(bool)), action, SLOT(setEnabled(bool)));
    m_keyReference->registerShortcut(action);
    menu->addAction(action);

    menu->addSeparator();

    m_keyReference->setCategory(tr("Zoom"));

    m_zoomInAction = new QAction(il.load("zoom-in"),
                                 tr("Zoom &In"), this);
    m_zoomInAction->setShortcut(tr("Up"));
    m_zoomInAction->setStatusTip(tr("Increase the zoom level"));
    connect(m_zoomInAction, SIGNAL(triggered()), this, SLOT(zoomIn()));
    connect(this, SIGNAL(canZoom(bool)), m_zoomInAction, SLOT(setEnabled(bool)));
    m_keyReference->registerShortcut(m_zoomInAction);
    menu->addAction(m_zoomInAction);
	
    m_zoomOutAction = new QAction(il.load("zoom-out"),
                                  tr("Zoom &Out"), this);
    m_zoomOutAction->setShortcut(tr("Down"));
    m_zoomOutAction->setStatusTip(tr("Decrease the zoom level"));
    connect(m_zoomOutAction, SIGNAL(triggered()), this, SLOT(zoomOut()));
    connect(this, SIGNAL(canZoom(bool)), m_zoomOutAction, SLOT(setEnabled(bool)));
    m_keyReference->registerShortcut(m_zoomOutAction);
    menu->addAction(m_zoomOutAction);
	
    action = new QAction(tr("Restore &Default Zoom"), this);
    action->setStatusTip(tr("Restore the zoom level to the default"));
    connect(action, SIGNAL(triggered()), this, SLOT(zoomDefault()));
    connect(this, SIGNAL(canZoom(bool)), action, SLOT(setEnabled(bool)));
    menu->addAction(action);

    m_zoomFitAction = new QAction(il.load("zoom-fit"),
                                  tr("Zoom to &Fit"), this);
    m_zoomFitAction->setShortcut(tr("F"));
    m_zoomFitAction->setStatusTip(tr("Zoom to show the whole file"));
    connect(m_zoomFitAction, SIGNAL(triggered()), this, SLOT(zoomToFit()));
    connect(this, SIGNAL(canZoom(bool)), m_zoomFitAction, SLOT(setEnabled(bool)));
    m_keyReference->registerShortcut(m_zoomFitAction);
    menu->addAction(m_zoomFitAction);

    menu->addSeparator();

    m_keyReference->setCategory(tr("Display Features"));

    action = new QAction(tr("Show &Centre Line"), this);
    action->setShortcut(tr("'"));
    action->setStatusTip(tr("Show or hide the centre line"));
    connect(action, SIGNAL(triggered()), this, SLOT(toggleCentreLine()));
    action->setCheckable(true);
    action->setChecked(true);
    m_keyReference->registerShortcut(action);
    menu->addAction(action);

    action = new QAction(tr("Toggle All Time Rulers"), this);
    action->setShortcut(tr("#"));
    action->setStatusTip(tr("Show or hide all time rulers"));
    connect(action, SIGNAL(triggered()), this, SLOT(toggleTimeRulers()));
    m_keyReference->registerShortcut(action);
    menu->addAction(action);

    menu->addSeparator();

    QActionGroup *overlayGroup = new QActionGroup(this);
        
    action = new QAction(tr("Show &No Overlays"), this);
    action->setShortcut(tr("0"));
    action->setStatusTip(tr("Hide times, layer names, and scale"));
    connect(action, SIGNAL(triggered()), this, SLOT(showNoOverlays()));
    action->setCheckable(true);
    action->setChecked(false);
    overlayGroup->addAction(action);
    m_keyReference->registerShortcut(action);
    menu->addAction(action);
        
    action = new QAction(tr("Show &Minimal Overlays"), this);
    action->setShortcut(tr("9"));
    action->setStatusTip(tr("Show times and basic scale"));
    connect(action, SIGNAL(triggered()), this, SLOT(showMinimalOverlays()));
    action->setCheckable(true);
    action->setChecked(true);
    overlayGroup->addAction(action);
    m_keyReference->registerShortcut(action);
    menu->addAction(action);
        
    action = new QAction(tr("Show &All Overlays"), this);
    action->setShortcut(tr("8"));
    action->setStatusTip(tr("Show times, layer names, and scale"));
    connect(action, SIGNAL(triggered()), this, SLOT(showAllOverlays()));
    action->setCheckable(true);
    action->setChecked(false);
    overlayGroup->addAction(action);
    m_keyReference->registerShortcut(action);
    menu->addAction(action);
        
    menu->addSeparator();

    action = new QAction(tr("Show &Zoom Wheels"), this);
    action->setShortcut(tr("Z"));
    action->setStatusTip(tr("Show thumbwheels for zooming horizontally and vertically"));
    connect(action, SIGNAL(triggered()), this, SLOT(toggleZoomWheels()));
    action->setCheckable(true);
    action->setChecked(m_viewManager->getZoomWheelsEnabled());
    m_keyReference->registerShortcut(action);
    menu->addAction(action);
        
    m_showPropertyBoxesAction = new QAction(tr("Show Property Bo&xes"), this);
    m_showPropertyBoxesAction->setShortcut(tr("X"));
    m_showPropertyBoxesAction->setStatusTip(tr("Show the layer property boxes at the side of the main window"));
    connect(m_showPropertyBoxesAction, SIGNAL(triggered()), this, SLOT(togglePropertyBoxes()));
    m_showPropertyBoxesAction->setCheckable(true);
    m_showPropertyBoxesAction->setChecked(false);
    m_keyReference->registerShortcut(m_showPropertyBoxesAction);
    menu->addAction(m_showPropertyBoxesAction);

    action = new QAction(tr("Show Status &Bar"), this);
    action->setStatusTip(tr("Show context help information in the status bar at the bottom of the window"));
    connect(action, SIGNAL(triggered()), this, SLOT(toggleStatusBar()));
    action->setCheckable(true);
    action->setChecked(true);
    menu->addAction(action);

    QSettings settings;
    settings.beginGroup("MainWindow");
    bool sb = settings.value("showstatusbar", true).toBool();
    if (!sb) {
        action->setChecked(false);
        statusBar()->hide();
    }
    settings.endGroup();

    menu->addSeparator();

    action = new QAction(tr("Show La&yer Hierarchy"), this);
    action->setShortcut(tr("H"));
    action->setStatusTip(tr("Open a window displaying the hierarchy of panes and layers in this session"));
    connect(action, SIGNAL(triggered()), this, SLOT(showLayerTree()));
    m_keyReference->registerShortcut(action);
    menu->addAction(action);

    menu->addSeparator();

    action = new QAction(tr("Go Full-Screen"), this);
    action->setShortcut(tr("F11"));
    action->setStatusTip(tr("Expand the pane area to the whole screen"));
    connect(action, SIGNAL(triggered()), this, SLOT(goFullScreen()));
    m_keyReference->registerShortcut(action);
    menu->addAction(action);
}

void
MainWindow::setupHelpMenu()
{
    QMenu *menu = menuBar()->addMenu(tr("&Help"));
    menu->setTearOffEnabled(false);
    
    m_keyReference->setCategory(tr("Help"));

    IconLoader il;

    QAction *action = new QAction(il.load("help"),
                                  tr("&Help Reference"), this); 
    action->setShortcut(tr("F1"));
    action->setStatusTip(tr("Open the reference manual")); 
    connect(action, SIGNAL(triggered()), this, SLOT(help()));
    m_keyReference->registerShortcut(action);
    menu->addAction(action);

    action = new QAction(tr("&Key and Mouse Reference"), this);
    action->setShortcut(tr("F2"));
    action->setStatusTip(tr("Open a window showing the keystrokes you can use"));
    connect(action, SIGNAL(triggered()), this, SLOT(keyReference()));
    m_keyReference->registerShortcut(action);
    menu->addAction(action);
    
    action = new QAction(tr("Sonic Vector on the &Web"), this); 
    action->setStatusTip(tr("Open the Sonic Vector website")); 
    connect(action, SIGNAL(triggered()), this, SLOT(website()));
    menu->addAction(action);
    
    action = new QAction(tr("&About Sonic Vector"), this); 
    action->setStatusTip(tr("Show information about Sonic Vector")); 
    connect(action, SIGNAL(triggered()), this, SLOT(about()));
    menu->addAction(action);
}

void
MainWindow::setupRecentFilesMenu()
{
    m_recentFilesMenu->clear();
    vector<QString> files = m_recentFiles.getRecent();
    for (size_t i = 0; i < files.size(); ++i) {
	QAction *action = new QAction(files[i], this);
	connect(action, SIGNAL(triggered()), this, SLOT(openRecentFile()));
        if (i == 0) {
            action->setShortcut(tr("Ctrl+R"));
            m_keyReference->registerShortcut
                (tr("Re-open"),
                 action->shortcut().toString(),
                 tr("Re-open the current or most recently opened file"));
        }
	m_recentFilesMenu->addAction(action);
    }
}

void
MainWindow::setupToolbars()
{
    m_keyReference->setCategory(tr("Playback and Transport Controls"));

    IconLoader il;

    QMenu *menu = m_playbackMenu = menuBar()->addMenu(tr("Play&back"));
    menu->setTearOffEnabled(false);
    m_rightButtonMenu->addSeparator();
    m_rightButtonPlaybackMenu = m_rightButtonMenu->addMenu(tr("Playback"));

    QToolBar *toolbar = addToolBar(tr("Playback Toolbar"));

    QAction *rwdStartAction = toolbar->addAction(il.load("rewind-start"),
                                                 tr("Rewind to Start"));
    rwdStartAction->setShortcut(tr("Home"));
    rwdStartAction->setStatusTip(tr("Rewind to the start"));
    connect(rwdStartAction, SIGNAL(triggered()), this, SLOT(rewindStart()));
    connect(this, SIGNAL(canPlay(bool)), rwdStartAction, SLOT(setEnabled(bool)));

    QAction *m_rwdAction = toolbar->addAction(il.load("rewind"),
                                              tr("Rewind"));
    m_rwdAction->setShortcut(tr("PgUp"));
    m_rwdAction->setStatusTip(tr("Rewind to the previous time instant or time ruler notch"));
    connect(m_rwdAction, SIGNAL(triggered()), this, SLOT(rewind()));
    connect(this, SIGNAL(canRewind(bool)), m_rwdAction, SLOT(setEnabled(bool)));

    m_playAction = toolbar->addAction(il.load("playpause"),
                                      tr("Play / Pause"));
    m_playAction->setCheckable(true);
    m_playAction->setShortcut(tr("Space"));
    m_playAction->setStatusTip(tr("Start or stop playback from the current position"));
    connect(m_playAction, SIGNAL(triggered()), this, SLOT(play()));
    connect(m_playSource, SIGNAL(playStatusChanged(bool)),
	    m_playAction, SLOT(setChecked(bool)));
    connect(this, SIGNAL(canPlay(bool)), m_playAction, SLOT(setEnabled(bool)));

    m_ffwdAction = toolbar->addAction(il.load("ffwd"),
                                              tr("Fast Forward"));
    m_ffwdAction->setShortcut(tr("PgDown"));
    m_ffwdAction->setStatusTip(tr("Fast-forward to the next time instant or time ruler notch"));
    connect(m_ffwdAction, SIGNAL(triggered()), this, SLOT(ffwd()));
    connect(this, SIGNAL(canFfwd(bool)), m_ffwdAction, SLOT(setEnabled(bool)));

    QAction *ffwdEndAction = toolbar->addAction(il.load("ffwd-end"),
                                                tr("Fast Forward to End"));
    ffwdEndAction->setShortcut(tr("End"));
    ffwdEndAction->setStatusTip(tr("Fast-forward to the end"));
    connect(ffwdEndAction, SIGNAL(triggered()), this, SLOT(ffwdEnd()));
    connect(this, SIGNAL(canPlay(bool)), ffwdEndAction, SLOT(setEnabled(bool)));
/*
    toolbar = addToolBar(tr("Play Mode Toolbar"));

    QAction *psAction = toolbar->addAction(il.load("playselection"),
                                           tr("Constrain Playback to Selection"));
    psAction->setCheckable(true);
    psAction->setChecked(m_viewManager->getPlaySelectionMode());
    psAction->setShortcut(tr("s"));
    psAction->setStatusTip(tr("Constrain playback to the selected regions"));
    connect(m_viewManager, SIGNAL(playSelectionModeChanged(bool)),
            psAction, SLOT(setChecked(bool)));
    connect(psAction, SIGNAL(triggered()), this, SLOT(playSelectionToggled()));
    connect(this, SIGNAL(canPlaySelection(bool)), psAction, SLOT(setEnabled(bool)));

    QAction *plAction = toolbar->addAction(il.load("playloop"),
                                           tr("Loop Playback"));
    plAction->setCheckable(true);
    plAction->setChecked(m_viewManager->getPlayLoopMode());
    plAction->setShortcut(tr("l"));
    plAction->setStatusTip(tr("Loop playback"));
    connect(m_viewManager, SIGNAL(playLoopModeChanged(bool)),
            plAction, SLOT(setChecked(bool)));
    connect(plAction, SIGNAL(triggered()), this, SLOT(playLoopToggled()));
    connect(this, SIGNAL(canPlay(bool)), plAction, SLOT(setEnabled(bool)));

    QAction *soAction = toolbar->addAction(il.load("solo"),
                                           tr("Solo Current Pane"));
    soAction->setCheckable(true);
    soAction->setChecked(m_viewManager->getPlaySoloMode());
    soAction->setShortcut(tr("o"));
    soAction->setStatusTip(tr("Solo the current pane during playback"));
    connect(m_viewManager, SIGNAL(playSoloModeChanged(bool)),
            soAction, SLOT(setChecked(bool)));
    connect(soAction, SIGNAL(triggered()), this, SLOT(playSoloToggled()));
    connect(this, SIGNAL(canPlay(bool)), soAction, SLOT(setEnabled(bool)));

    m_keyReference->registerShortcut(psAction);
    m_keyReference->registerShortcut(plAction);
    m_keyReference->registerShortcut(soAction);
*/


    QAction *alAction = 0;
    alAction = toolbar->addAction(il.load("align"),
                                  tr("Align File Timelines"));
    alAction->setCheckable(true);
    alAction->setChecked(m_viewManager->getAlignMode());
    alAction->setStatusTip(tr("Treat multiple audio files as versions of the same work, and align their timelines"));
    connect(m_viewManager, SIGNAL(alignModeChanged(bool)),
            alAction, SLOT(setChecked(bool)));
    connect(alAction, SIGNAL(triggered()), this, SLOT(alignToggled()));

    m_keyReference->registerShortcut(m_playAction);
    m_keyReference->registerShortcut(m_rwdAction);
    m_keyReference->registerShortcut(m_ffwdAction);
    m_keyReference->registerShortcut(rwdStartAction);
    m_keyReference->registerShortcut(ffwdEndAction);

/*
    menu->addAction(psAction);
    menu->addAction(plAction);
    menu->addAction(soAction);
*/
    menu->addAction(m_playAction);
    menu->addSeparator();
    menu->addAction(m_rwdAction);
    menu->addAction(m_ffwdAction);
    menu->addSeparator();
    menu->addAction(rwdStartAction);
    menu->addAction(ffwdEndAction);
    menu->addSeparator();
    menu->addAction(alAction);
    menu->addSeparator();

    m_rightButtonPlaybackMenu->addAction(m_playAction);
/*
    m_rightButtonPlaybackMenu->addAction(psAction);
    m_rightButtonPlaybackMenu->addAction(plAction);
    m_rightButtonPlaybackMenu->addAction(soAction);
*/
    m_rightButtonPlaybackMenu->addSeparator();
    m_rightButtonPlaybackMenu->addAction(m_rwdAction);
    m_rightButtonPlaybackMenu->addAction(m_ffwdAction);
    m_rightButtonPlaybackMenu->addSeparator();
    m_rightButtonPlaybackMenu->addAction(rwdStartAction);
    m_rightButtonPlaybackMenu->addAction(ffwdEndAction);
    m_rightButtonPlaybackMenu->addSeparator();
    m_rightButtonPlaybackMenu->addAction(alAction);
    m_rightButtonPlaybackMenu->addSeparator();

    QAction *fastAction = menu->addAction(tr("Speed Up"));
    fastAction->setShortcut(tr("Ctrl+PgUp"));
    fastAction->setStatusTip(tr("Time-stretch playback to speed it up without changing pitch"));
    connect(fastAction, SIGNAL(triggered()), this, SLOT(speedUpPlayback()));
    connect(this, SIGNAL(canSpeedUpPlayback(bool)), fastAction, SLOT(setEnabled(bool)));
    
    QAction *slowAction = menu->addAction(tr("Slow Down"));
    slowAction->setShortcut(tr("Ctrl+PgDown"));
    slowAction->setStatusTip(tr("Time-stretch playback to slow it down without changing pitch"));
    connect(slowAction, SIGNAL(triggered()), this, SLOT(slowDownPlayback()));
    connect(this, SIGNAL(canSlowDownPlayback(bool)), slowAction, SLOT(setEnabled(bool)));

    QAction *normalAction = menu->addAction(tr("Restore Normal Speed"));
    normalAction->setShortcut(tr("Ctrl+Home"));
    normalAction->setStatusTip(tr("Restore non-time-stretched playback"));
    connect(normalAction, SIGNAL(triggered()), this, SLOT(restoreNormalPlayback()));
    connect(this, SIGNAL(canChangePlaybackSpeed(bool)), normalAction, SLOT(setEnabled(bool)));

    m_keyReference->registerShortcut(fastAction);
    m_keyReference->registerShortcut(slowAction);
    m_keyReference->registerShortcut(normalAction);

    m_rightButtonPlaybackMenu->addAction(fastAction);
    m_rightButtonPlaybackMenu->addAction(slowAction);
    m_rightButtonPlaybackMenu->addAction(normalAction);
/*
    toolbar = addToolBar(tr("Edit Toolbar"));
    CommandHistory::getInstance()->registerToolbar(toolbar);
*/

    Pane::registerShortcuts(*m_keyReference);
}

void
MainWindow::updateMenuStates()
{
    MainWindowBase::updateMenuStates();

    Pane *currentPane = 0;
    Layer *currentLayer = 0;

    if (m_paneStack) currentPane = m_paneStack->getCurrentPane();
    if (currentPane) currentLayer = currentPane->getSelectedLayer();

    bool haveCurrentPane =
        (currentPane != 0);
    bool haveCurrentLayer =
        (haveCurrentPane &&
         (currentLayer != 0));
    bool haveCurrentTimeInstantsLayer = 
	(haveCurrentLayer &&
	 dynamic_cast<TimeInstantLayer *>(currentLayer));
    bool haveCurrentTimeValueLayer = 
	(haveCurrentLayer &&
	 dynamic_cast<TimeValueLayer *>(currentLayer));

    emit canChangePlaybackSpeed(true);
    int v = m_playSpeed->value();
    emit canSpeedUpPlayback(v < m_playSpeed->maximum());
    emit canSlowDownPlayback(v > m_playSpeed->minimum());

    if (m_ffwdAction && m_rwdAction) {
        if (haveCurrentTimeInstantsLayer) {
            m_ffwdAction->setText(tr("Fast Forward to Next Instant"));
            m_ffwdAction->setStatusTip(tr("Fast forward to the next time instant in the current layer"));
            m_rwdAction->setText(tr("Rewind to Previous Instant"));
            m_rwdAction->setStatusTip(tr("Rewind to the previous time instant in the current layer"));
        } else if (haveCurrentTimeValueLayer) {
            m_ffwdAction->setText(tr("Fast Forward to Next Point"));
            m_ffwdAction->setStatusTip(tr("Fast forward to the next point in the current layer"));
            m_rwdAction->setText(tr("Rewind to Previous Point"));
            m_rwdAction->setStatusTip(tr("Rewind to the previous point in the current layer"));
        } else {
            m_ffwdAction->setText(tr("Fast Forward"));
            m_ffwdAction->setStatusTip(tr("Fast forward"));
            m_rwdAction->setText(tr("Rewind"));
            m_rwdAction->setStatusTip(tr("Rewind"));
        }
    }
}

void
MainWindow::updateDescriptionLabel()
{
    if (!getMainModel()) {
	return;
    }

    QString description;

    sv_samplerate_t ssr = getMainModel()->getSampleRate();
    sv_samplerate_t tsr = ssr;
    if (m_playSource) tsr = m_playSource->getDeviceSampleRate();

    if (ssr != tsr) {
	description = tr("%1Hz (resampling to %2Hz)").arg(ssr).arg(tsr);
    } else {
	description = QString("%1Hz").arg(ssr);
    }

    description = QString("%1 - %2")
	.arg(RealTime::frame2RealTime(getMainModel()->getEndFrame(), ssr)
	     .toText(false).c_str())
	.arg(description);

    //!!! but we don't actually have a description label
}

void
MainWindow::documentModified()
{
    //!!!
    MainWindowBase::documentModified();
}

void
MainWindow::documentRestored()
{
    //!!!
    MainWindowBase::documentRestored();
}

void
MainWindow::selectMainPane()
{
    if (m_paneStack && m_paneStack->getPaneCount() > 0) {
        m_paneStack->setCurrentPane(m_paneStack->getPane(0));
    }
}

void
MainWindow::newSession()
{
    if (!checkSaveModified()) return;

    cerr << "MainWindow::newSession" << endl;

    closeSession();
    createDocument();

    // We need a pane, so that we have something to receive drop events
    
    Pane *pane = m_paneStack->addPane();

    connect(pane, SIGNAL(contextHelpChanged(const QString &)),
            this, SLOT(contextHelpChanged(const QString &)));

    m_overview->registerView(pane);

    m_document->setAutoAlignment(m_viewManager->getAlignMode());

    CommandHistory::getInstance()->clear();
    CommandHistory::getInstance()->documentSaved();
    documentRestored();
    updateMenuStates();
}

void
MainWindow::closeSession()
{
    if (!checkSaveModified()) return;

    while (m_paneStack->getPaneCount() > 0) {

	Pane *pane = m_paneStack->getPane(m_paneStack->getPaneCount() - 1);

	while (pane->getLayerCount() > 0) {
	    m_document->removeLayerFromView
		(pane, pane->getLayer(pane->getLayerCount() - 1));
	}

	m_overview->unregisterView(pane);
	m_paneStack->deletePane(pane);
    }

    while (m_paneStack->getHiddenPaneCount() > 0) {

	Pane *pane = m_paneStack->getHiddenPane
	    (m_paneStack->getHiddenPaneCount() - 1);

	while (pane->getLayerCount() > 0) {
	    m_document->removeLayerFromView
		(pane, pane->getLayer(pane->getLayerCount() - 1));
	}

	m_overview->unregisterView(pane);
	m_paneStack->deletePane(pane);
    }

    delete m_document;
    m_document = 0;
    m_viewManager->clearSelections();
    m_timeRulerLayer = 0; // document owned this

    m_sessionFile = "";
    setWindowTitle(tr("Sonic Vector"));

    CommandHistory::getInstance()->clear();
    CommandHistory::getInstance()->documentSaved();
    documentRestored();
}

void
MainWindow::openFile()
{
    QString orig = m_audioFile;
    if (orig == "") orig = ".";
    else orig = QFileInfo(orig).absoluteDir().canonicalPath();

    QString path = getOpenFileName(FileFinder::AnyFile);

    if (path.isEmpty()) return;

    FileOpenStatus status = openPath(path, CreateAdditionalModel);

    if (status == FileOpenFailed) {
        QMessageBox::critical(this, tr("Failed to open file"),
                              tr("<b>File open failed</b><p>File \"%1\" could not be opened").arg(path));
    } else if (status == FileOpenWrongMode) {
        QMessageBox::critical(this, tr("Failed to open file"),
                              tr("<b>Audio required</b><p>Please load at least one audio file before importing annotation data"));
    } else {
        configureNewPane(m_paneStack->getCurrentPane());
    }
}

void
MainWindow::openLocation()
{
    QSettings settings;
    settings.beginGroup("MainWindow");
    QString lastLocation = settings.value("lastremote", "").toString();

    bool ok = false;
    QString text = QInputDialog::getText
        (this, tr("Open Location"),
         tr("Please enter the URL of the location to open:"),
         QLineEdit::Normal, lastLocation, &ok);

    if (!ok) return;

    settings.setValue("lastremote", text);

    if (text.isEmpty()) return;

    FileOpenStatus status = openPath(text, CreateAdditionalModel);

    if (status == FileOpenFailed) {
        QMessageBox::critical(this, tr("Failed to open location"),
                              tr("<b>Open failed</b><p>URL \"%1\" could not be opened").arg(text));
    } else if (status == FileOpenWrongMode) {
        QMessageBox::critical(this, tr("Failed to open location"),
                              tr("<b>Audio required</b><p>Please load at least one audio file before importing annotation data"));
    } else {
        configureNewPane(m_paneStack->getCurrentPane());
    }
}

void
MainWindow::openRecentFile()
{
    QObject *obj = sender();
    QAction *action = dynamic_cast<QAction *>(obj);
    
    if (!action) {
	cerr << "WARNING: MainWindow::openRecentFile: sender is not an action"
		  << endl;
	return;
    }

    QString path = action->text();
    if (path == "") return;


    cerr << "about to call open(), have " << m_paneStack->getPaneCount() << " panes" << endl;

    FileOpenStatus status = openPath(path, CreateAdditionalModel);

    cerr << "called open(), have " << m_paneStack->getPaneCount() << " panes" << endl;

    if (status == FileOpenFailed) {
        QMessageBox::critical(this, tr("Failed to open location"),
                              tr("<b>Open failed</b><p>File or URL \"%1\" could not be opened").arg(path));
    } else if (status == FileOpenWrongMode) {
        QMessageBox::critical(this, tr("Failed to open location"),
                              tr("<b>Audio required</b><p>Please load at least one audio file before importing annotation data"));
    } else {
        configureNewPane(m_paneStack->getCurrentPane());
    }
}

Model *
MainWindow::selectExistingLayerForMode(Pane *pane, QString name)
{   
    // Hides all layers in the given pane that have names differing
    // from the given name, except for time instants layers (which are
    // assumed to be used for segment display). If a layer is found
    // that has the given name, shows that layer and returns 0. If no
    // layer is found with the given name, returns a pointer to the
    // model from which such a layer should be constructed.

    Model *model = 0;

    bool have = false;

    for (int i = 0; i < pane->getLayerCount(); ++i) {
        
        Layer *layer = pane->getLayer(i);
        if (!layer || qobject_cast<TimeInstantLayer *>(layer)) continue;
        
        Model *lm = layer->getModel();
        while (lm && lm->getSourceModel()) lm = lm->getSourceModel();
        if (qobject_cast<WaveFileModel *>(lm)) model = lm;
        
        QString ln = layer->objectName();
        if (ln != name) {
            m_hiddenLayers[pane].insert(layer);
            m_document->removeLayerFromView(pane, layer);
            continue;
        }
        
        have = true;
    }
    
    if (have) return 0;

    LayerSet &ls = m_hiddenLayers[pane];
    bool found = false;
    for (LayerSet::iterator i = ls.begin(); i != ls.end(); ++i) {
        if ((*i)->objectName() == name) {
            m_document->addLayerToView(pane, *i);
            ls.erase(i);
            found = true;
            break;
        }
    }

    if (found) return 0;

    return model;
}

void
MainWindow::addSalientFeatureLayer(Pane *pane, WaveFileModel *model)
{
    //!!! what if there already is one? could have changed the main
    //!!! model for example

    if (!model) {
        cerr << "MainWindow::addSalientFeatureLayer: No model" << endl;
        return;
    }
    
    TransformFactory *tf = TransformFactory::getInstance();
    if (!tf) {
        cerr << "Failed to locate a transform factory!" << endl;
        return;
    }
    
//    TransformId id = "vamp:qm-vamp-plugins:qm-keydetector:key";
    TransformId id = "vamp:nnls-chroma:chordino:simplechord";
    if (!tf->haveTransform(id)) {
        cerr << "No plugin available for salient feature layer; transform is: "
             << id << endl;
        return;
    }

    if (!model) {
        return;
    }

    m_salientCalculating = true;

    Transform transform = tf->getDefaultTransformFor
        (id, model->getSampleRate());

    transform.setStepSize(1024);
    transform.setBlockSize(2048);

    ModelTransformer::Input input(model, -1);

    Layer *newLayer = m_document->createDerivedLayer(transform, model);

    if (newLayer) {

        TimeInstantLayer *til = qobject_cast<TimeInstantLayer *>(newLayer);
        if (til) til->setPlotStyle(TimeInstantLayer::PlotInstants);

        connect(til, SIGNAL(modelCompletionChanged()),
                this, SLOT(salientLayerCompletionChanged()));
        
        m_document->addLayerToView(pane, newLayer);
        m_paneStack->setCurrentLayer(pane, newLayer);
    }
}

void
MainWindow::salientLayerCompletionChanged()
{
    Layer *layer = qobject_cast<Layer *>(sender());
    cerr << "MainWindow::salientLayerCompletionChanged: layer = " << layer << endl;
    if (layer && layer->getCompletion(0) == 100) {
        m_salientCalculating = false;
        cerr << "mapping " << m_salientPending.size() << " salient layer(s)" << endl;
        foreach (AlignmentModel *am, m_salientPending) {
            mapSalientFeatureLayer(am);
        }
        m_salientPending.clear();
    }
}

TimeInstantLayer *
MainWindow::findSalientFeatureLayer()
{
    if (!getMainModel()) return 0;

    for (int i = 0; i < m_paneStack->getPaneCount(); ++i) {
        Pane *p = m_paneStack->getPane(i);

        for (int j = 0; j < p->getLayerCount(); ++j) {
            Layer *l = p->getLayer(j);

            if (l->getModel() == getMainModel()) {

                for (int k = 0; k < p->getLayerCount(); ++k) {
                    TimeInstantLayer *ll = qobject_cast<TimeInstantLayer *>
                        (p->getLayer(k));
                    if (ll) return ll;
                }
            }
        }
    }

    return 0;
}

void
MainWindow::mapSalientFeatureLayer(AlignmentModel *am)
{
    if (m_salientCalculating) {
        m_salientPending.insert(am);
        return;
    }

    TimeInstantLayer *salient = findSalientFeatureLayer();
    if (!salient) {
        cerr << "MainWindow::mapSalientFeatureLayer: No salient layer found"
             << endl;
        m_salientPending.insert(am);
        return;
    }

    if (!am) {
        cerr << "MainWindow::mapSalientFeatureLayer: AlignmentModel is null!" << endl;
        return;
    }
    
    const Model *model = am->getAlignedModel();

    Pane *pane = 0;
    Layer *layer = 0;
    
    for (int i = 0; i < m_paneStack->getPaneCount(); ++i) {
        Pane *p = m_paneStack->getPane(i);
        for (int j = 0; j < p->getLayerCount(); ++j) {
            Layer *l = p->getLayer(j);
            if (l && (l->getModel() == model)) {
                pane = p;
                layer = l;
                break;
            }
        }
    }

    if (!pane || !layer) {
        cerr << "MainWindow::mapSalientFeatureLayer: Failed to find model "
             << model << " in any layer" << endl;
        return;
    }

    //!!! command?

    const SparseOneDimensionalModel *from =
        qobject_cast<const SparseOneDimensionalModel *>(salient->getModel());
    if (!from) {
        cerr << "MainWindow::mapSalientFeatureLayer: Salient layer lacks SparseOneDimensionalModel" << endl;
        return;
    }
        
    SparseOneDimensionalModel *to = new SparseOneDimensionalModel
        (model->getSampleRate(), from->getResolution(), false);
    
    SparseOneDimensionalModel::PointList pp = from->getPoints();
    foreach (SparseOneDimensionalModel::Point p, pp) {
        p.frame = am->fromReference(p.frame);
        to->addPoint(p);
    }

    Layer *newLayer = m_document->createImportedLayer(to);

    if (newLayer) {

        TimeInstantLayer *til = qobject_cast<TimeInstantLayer *>(newLayer);
        if (til) til->setPlotStyle(TimeInstantLayer::PlotInstants);
        
        m_document->addLayerToView(pane, newLayer);
        m_paneStack->setCurrentLayer(pane, newLayer);
    }
}

void
MainWindow::curveModeSelected()
{
    QString name = tr("Curve");

    for (int i = 0; i < m_paneStack->getPaneCount(); ++i) {

        Pane *pane = m_paneStack->getPane(i);
        if (!pane) continue;

        Model *model = selectExistingLayerForMode(pane, name);
        if (!model) continue;

        TransformId id = "vamp:qm-vamp-plugins:qm-onsetdetector:detection_fn";
        TransformFactory *tf = TransformFactory::getInstance();

        if (tf->haveTransform(id)) {

            Transform transform = tf->getDefaultTransformFor
                (id, model->getSampleRate());

            transform.setStepSize(1024);
            transform.setBlockSize(2048);

            ModelTransformer::Input input(model, -1);

//!!! no equivalent for this yet            context.updates = false;

            Layer *newLayer = m_document->createDerivedLayer(transform, model);

            if (newLayer) {
                newLayer->setObjectName(name);
                m_document->addLayerToView(pane, newLayer);
                m_paneStack->setCurrentLayer(pane, newLayer);
            }
            
        } else {
            cerr << "No onset detector plugin available" << endl;
        }
    }

    m_displayMode = CurveMode;
}

void
MainWindow::waveformModeSelected()
{
    QString name = tr("Waveform");

    for (int i = 0; i < m_paneStack->getPaneCount(); ++i) {

        Pane *pane = m_paneStack->getPane(i);
        if (!pane) continue;

        Model *model = selectExistingLayerForMode(pane, name);
        if (!model) continue;

        Layer *newLayer = m_document->createLayer(LayerFactory::Waveform);
        newLayer->setObjectName(name);
        m_document->setModel(newLayer, model);
        m_document->addLayerToView(pane, newLayer);
        m_paneStack->setCurrentLayer(pane, newLayer);
    }

    m_displayMode = WaveformMode;
}

void
MainWindow::spectrogramModeSelected()
{
    QString name = tr("Spectrogram");

    for (int i = 0; i < m_paneStack->getPaneCount(); ++i) {

        Pane *pane = m_paneStack->getPane(i);
        if (!pane) continue;

        Model *model = selectExistingLayerForMode(pane, name);
        if (!model) continue;
        
        Layer *newLayer = m_document->createLayer(LayerFactory::Spectrogram);
        newLayer->setObjectName(name);
        m_document->setModel(newLayer, model);
        m_document->addLayerToView(pane, newLayer);
        m_paneStack->setCurrentLayer(pane, newLayer);
    }

    m_displayMode = SpectrogramMode;
}

void
MainWindow::melodogramModeSelected()
{
    QString name = tr("Melodic Range Spectrogram");

    for (int i = 0; i < m_paneStack->getPaneCount(); ++i) {

        Pane *pane = m_paneStack->getPane(i);
        if (!pane) continue;

        Model *model = selectExistingLayerForMode(pane, name);
        if (!model) continue;

        Layer *newLayer = m_document->createLayer
            (LayerFactory::MelodicRangeSpectrogram);
        newLayer->setObjectName(name);
        m_document->setModel(newLayer, model);
        m_document->addLayerToView(pane, newLayer);
        m_paneStack->setCurrentLayer(pane, newLayer);
    }

    m_displayMode = MelodogramMode;
}

void
MainWindow::reselectMode()
{
    switch (m_displayMode) {
    case CurveMode: curveModeSelected(); break;
    case WaveformMode: waveformModeSelected(); break;
    case SpectrogramMode: spectrogramModeSelected(); break;
    case MelodogramMode: melodogramModeSelected(); break;
    }
}

void
MainWindow::paneAdded(Pane *pane)
{
    pane->setPlaybackFollow(PlaybackScrollContinuous);
    m_paneStack->sizePanesEqually();
    if (m_overview) m_overview->registerView(pane);
}    

void
MainWindow::paneHidden(Pane *pane)
{
    if (m_overview) m_overview->unregisterView(pane); 
}    

void
MainWindow::paneAboutToBeDeleted(Pane *pane)
{
    if (m_overview) m_overview->unregisterView(pane); 
}    

void
MainWindow::paneDropAccepted(Pane * /* pane */, QStringList uriList)
{
    if (uriList.empty()) return;

    QUrl first(uriList[0]);

    cerr << "uriList.size() == " << uriList.size() << endl;
    cerr << "first.isLocalFile() == " << first.isLocalFile() << endl;
    cerr << "QFileInfo(first.path()).isDir() == " << QFileInfo(first.path()).isDir() << endl;
    
    if (uriList.size() == 1 &&
        first.isLocalFile() &&
        QFileInfo(first.path()).isDir()) {

        FileOpenStatus status = openDirOfAudio(first.path());

        if (status != FileOpenSucceeded) {
            QMessageBox::critical(this, tr("Failed to open dropped URL"),
                                  tr("<b>Open failed</b><p>Dropped URL \"%1\" could not be opened").arg(uriList[0]));
        }

        return;
    }
    
    for (QStringList::iterator i = uriList.begin(); i != uriList.end(); ++i) {

        FileOpenStatus status = openPath(*i, CreateAdditionalModel);

        if (status == FileOpenFailed) {
            QMessageBox::critical(this, tr("Failed to open dropped URL"),
                                  tr("<b>Open failed</b><p>Dropped URL \"%1\" could not be opened").arg(*i));
        } else if (status == FileOpenWrongMode) {
            QMessageBox::critical(this, tr("Failed to open dropped URL"),
                                  tr("<b>Audio required</b><p>Please load at least one audio file before importing annotation data"));
        } else {
            configureNewPane(m_paneStack->getCurrentPane());
        }
    }
}

void
MainWindow::paneDropAccepted(Pane *pane, QString text)
{
    if (pane) m_paneStack->setCurrentPane(pane);

    QUrl testUrl(text);
    if (testUrl.scheme() == "file" || 
        testUrl.scheme() == "http" || 
        testUrl.scheme() == "ftp") {
        QStringList list;
        list.push_back(text);
        paneDropAccepted(pane, list);
        return;
    }

    //!!! open as text -- but by importing as if a CSV, or just adding
    //to a text layer?
}

void
MainWindow::configureNewPane(Pane *pane)
{
    cerr << "MainWindow::configureNewPane(" << pane << ")" << endl;

    if (!pane) return;

    Layer *waveformLayer = 0;

    for (int i = 0; i < pane->getLayerCount(); ++i) {
        Layer *layer = pane->getLayer(i);
        if (!layer) continue;
        if (dynamic_cast<WaveformLayer *>(layer)) waveformLayer = layer;
        if (dynamic_cast<TimeValueLayer *>(layer)) return;
    }
    if (!waveformLayer) return;

    waveformLayer->setObjectName(tr("Waveform"));

    zoomToFit();
    reselectMode();
}

void
MainWindow::closeEvent(QCloseEvent *e)
{
    if (m_exiting) {
        e->accept();
        return;
    }

//    cerr << "MainWindow::closeEvent" << endl;

    if (m_openingAudioFile) {
//        cerr << "Busy - ignoring close event" << endl;
	e->ignore();
	return;
    }

    if (!m_abandoning && !checkSaveModified()) {
//        cerr << "Ignoring close event" << endl;
	e->ignore();
	return;
    }

    QSettings settings;
    settings.beginGroup("MainWindow");
    settings.setValue("maximised", isMaximized());
    if (!isMaximized()) {
        settings.setValue("size", size());
        settings.setValue("position", pos());
    }
    settings.endGroup();

    delete m_keyReference;
    m_keyReference = 0;

    if (m_preferencesDialog &&
        m_preferencesDialog->isVisible()) {
        closeSession(); // otherwise we'll have to wait for prefs changes
        m_preferencesDialog->applicationClosing(false);
    }

    if (m_layerTreeView &&
        m_layerTreeView->isVisible()) {
        delete m_layerTreeView;
    }

    closeSession();

    e->accept();

    m_exiting = true;
    qApp->closeAllWindows();
    
    return;
}

bool
MainWindow::commitData(bool mayAskUser)
{
    if (mayAskUser) {
        bool rv = checkSaveModified();
        if (rv) {
            if (m_preferencesDialog &&
                m_preferencesDialog->isVisible()) {
                m_preferencesDialog->applicationClosing(false);
            }
        }
        return rv;
    } else {
        if (m_preferencesDialog &&
            m_preferencesDialog->isVisible()) {
            m_preferencesDialog->applicationClosing(true);
        }
        if (!m_documentModified) return true;

        // If we can't check with the user first, then we can't save
        // to the original session file (even if we have it) -- have
        // to use a temporary file

        QString svDirBase = ".sv1";
        QString svDir = QDir::home().filePath(svDirBase);

        if (!QFileInfo(svDir).exists()) {
            if (!QDir::home().mkdir(svDirBase)) return false;
        } else {
            if (!QFileInfo(svDir).isDir()) return false;
        }
        
        // This name doesn't have to be unguessable
#ifndef _WIN32
        QString fname = QString("tmp-%1-%2.sv")
            .arg(QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz"))
            .arg(QProcess().pid());
#else
        QString fname = QString("tmp-%1.sv")
            .arg(QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz"));
#endif
        QString fpath = QDir(svDir).filePath(fname);
        if (saveSessionFile(fpath)) {
            m_recentFiles.addFile(fpath);
            return true;
        } else {
            return false;
        }
    }
}

bool
MainWindow::checkSaveModified()
{
    // Called before some destructive operation (e.g. new session,
    // exit program).  Return true if we can safely proceed, false to
    // cancel.

    if (!m_documentModified) return true;

    int button = 
	QMessageBox::warning(this,
			     tr("Session modified"),
			     tr("The current session has been modified.\nDo you want to save it?"),
			     QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                             QMessageBox::Yes);

    if (button == QMessageBox::Yes) {
	saveSession();
	if (m_documentModified) { // save failed -- don't proceed!
	    return false;
	} else {
            return true; // saved, so it's safe to continue now
        }
    } else if (button == QMessageBox::No) {
	m_documentModified = false; // so we know to abandon it
	return true;
    }

    // else cancel
    return false;
}

void
MainWindow::saveSession()
{
    if (m_sessionFile != "") {
	if (!saveSessionFile(m_sessionFile)) {
	    QMessageBox::critical(this, tr("Failed to save file"),
				  tr("Session file \"%1\" could not be saved.").arg(m_sessionFile));
	} else {
	    CommandHistory::getInstance()->documentSaved();
	    documentRestored();
	}
    } else {
	saveSessionAs();
    }
}

void
MainWindow::saveSessionAs()
{
    QString orig = m_audioFile;
    if (orig == "") orig = ".";
    else orig = QFileInfo(orig).absoluteDir().canonicalPath();

    QString path = getSaveFileName(FileFinder::SessionFile);

    if (path == "") return;

    if (!saveSessionFile(path)) {
	QMessageBox::critical(this, tr("Failed to save file"),
			      tr("Session file \"%1\" could not be saved.").arg(path));
    } else {
	setWindowTitle(tr("%1: %2")
                       .arg(QApplication::applicationName())
		       .arg(QFileInfo(path).fileName()));
	m_sessionFile = path;
	CommandHistory::getInstance()->documentSaved();
	documentRestored();
        m_recentFiles.addFile(path);
    }
}

void
MainWindow::preferenceChanged(PropertyContainer::PropertyName name)
{
    MainWindowBase::preferenceChanged(name);

    if (name == "Background Mode" && m_viewManager) {
        if (m_viewManager->getGlobalDarkBackground()) {
            m_panLayer->setBaseColour
                (ColourDatabase::getInstance()->getColourIndex(tr("Bright Green")));
        } else {
            m_panLayer->setBaseColour
                (ColourDatabase::getInstance()->getColourIndex(tr("Green")));
        }      
    }    
}

void
MainWindow::renameCurrentLayer()
{
    Pane *pane = m_paneStack->getCurrentPane();
    if (pane) {
	Layer *layer = pane->getSelectedLayer();
	if (layer) {
	    bool ok = false;
	    QString newName = QInputDialog::getText
		(this, tr("Rename Layer"),
		 tr("New name for this layer:"),
		 QLineEdit::Normal, layer->objectName(), &ok);
	    if (ok) {
		layer->setObjectName(newName);
	    }
	}
    }
}

void
MainWindow::alignToggled()
{
    QAction *action = dynamic_cast<QAction *>(sender());
    
    if (!m_viewManager) return;

    if (action) {
	m_viewManager->setAlignMode(action->isChecked());
    } else {
	m_viewManager->setAlignMode(!m_viewManager->getAlignMode());
    }

    if (m_viewManager->getAlignMode()) {
        m_document->alignModels();
        m_document->setAutoAlignment(true);
    } else {
        m_document->setAutoAlignment(false);
    }

    for (int i = 0; i < m_paneStack->getPaneCount(); ++i) {
	Pane *pane = m_paneStack->getPane(i);
	if (!pane) continue;
        pane->update();
    }
}

void
MainWindow::playSpeedChanged(int position)
{
    PlaySpeedRangeMapper mapper;

    double percent = m_playSpeed->mappedValue();
    double factor = mapper.getFactorForValue(percent);

//    cerr << "play speed position = " << position << " (range 0-120) percent = " << percent << " factor = " << factor << endl;

    int centre = m_playSpeed->defaultValue();

    // Percentage is shown to 0dp if >100, to 1dp if <100; factor is
    // shown to 3sf

    char pcbuf[30];
    char facbuf[30];
    
    if (position == centre) {
        contextHelpChanged(tr("Playback speed: Normal"));
    } else if (position < centre) {
        sprintf(pcbuf, "%.1f", percent);
        sprintf(facbuf, "%.3g", 1.0 / factor);
        contextHelpChanged(tr("Playback speed: %1% (%2x slower)")
                           .arg(pcbuf)
                           .arg(facbuf));
    } else {
        sprintf(pcbuf, "%.0f", percent);
        sprintf(facbuf, "%.3g", factor);
        contextHelpChanged(tr("Playback speed: %1% (%2x faster)")
                           .arg(pcbuf)
                           .arg(facbuf));
    }

    m_playSource->setTimeStretch(1.0 / factor); // factor is a speedup

    updateMenuStates();
}

void
MainWindow::speedUpPlayback()
{
    int value = m_playSpeed->value();
    value = value + m_playSpeed->pageStep();
    if (value > m_playSpeed->maximum()) value = m_playSpeed->maximum();
    m_playSpeed->setValue(value);
}

void
MainWindow::slowDownPlayback()
{
    int value = m_playSpeed->value();
    value = value - m_playSpeed->pageStep();
    if (value < m_playSpeed->minimum()) value = m_playSpeed->minimum();
    m_playSpeed->setValue(value);
}

void
MainWindow::restoreNormalPlayback()
{
    m_playSpeed->setValue(m_playSpeed->defaultValue());
}

void
MainWindow::updateVisibleRangeDisplay(Pane *p) const
{
    if (!getMainModel() || !p) {
        return;
    }

    bool haveSelection = false;
    sv_frame_t startFrame = 0, endFrame = 0;

    if (m_viewManager && m_viewManager->haveInProgressSelection()) {

        bool exclusive = false;
        Selection s = m_viewManager->getInProgressSelection(exclusive);

        if (!s.isEmpty()) {
            haveSelection = true;
            startFrame = s.getStartFrame();
            endFrame = s.getEndFrame();
        }
    }

    if (!haveSelection) {
        startFrame = p->getFirstVisibleFrame();
        endFrame = p->getLastVisibleFrame();
    }

    RealTime start = RealTime::frame2RealTime
        (startFrame, getMainModel()->getSampleRate());

    RealTime end = RealTime::frame2RealTime
        (endFrame, getMainModel()->getSampleRate());

    RealTime duration = end - start;

    QString startStr, endStr, durationStr;
    startStr = start.toText(true).c_str();
    endStr = end.toText(true).c_str();
    durationStr = duration.toText(true).c_str();

    if (haveSelection) {
        m_myStatusMessage = tr("Selection: %1 to %2 (duration %3)")
            .arg(startStr).arg(endStr).arg(durationStr);
    } else {
        m_myStatusMessage = tr("Visible: %1 to %2 (duration %3)")
            .arg(startStr).arg(endStr).arg(durationStr);
    }

    statusBar()->showMessage(m_myStatusMessage);
}

void
MainWindow::updatePositionStatusDisplays() const
{
    if (!statusBar()->isVisible()) return;

}

void
MainWindow::monitoringLevelsChanged(float left, float right)
{
    m_mainLevelPan->setMonitoringLevels(left, right);
}

void
MainWindow::sampleRateMismatch(sv_samplerate_t requested,
                               sv_samplerate_t actual,
                               bool willResample)
{
    if (!willResample) {
        //!!! more helpful message needed
        QMessageBox::information
            (this, tr("Sample rate mismatch"),
             tr("The sample rate of this audio file (%1 Hz) does not match\nthe current playback rate (%2 Hz).\n\nThe file will play at the wrong speed and pitch.")
             .arg(requested).arg(actual));
    }        

    updateDescriptionLabel();
}

void
MainWindow::audioOverloadPluginDisabled()
{
    QMessageBox::information
        (this, tr("Audio processing overload"),
         tr("<b>Overloaded</b><p>Audio effects plugin auditioning has been disabled due to a processing overload."));
}

void
MainWindow::audioTimeStretchMultiChannelDisabled()
{
    static bool shownOnce = false;
    if (shownOnce) return;
    QMessageBox::information
        (this, tr("Audio processing overload"),
         tr("<b>Overloaded</b><p>Audio playback speed processing has been reduced to a single channel, due to a processing overload."));
    shownOnce = true;
}

void
MainWindow::layerRemoved(Layer *layer)
{
    MainWindowBase::layerRemoved(layer);
}

void
MainWindow::layerInAView(Layer *layer, bool inAView)
{
    MainWindowBase::layerInAView(layer, inAView);
}

void
MainWindow::modelAdded(Model *model)
{
    MainWindowBase::modelAdded(model);
}

void
MainWindow::modelAboutToBeDeleted(Model *model)
{
    MainWindowBase::modelAboutToBeDeleted(model);
}

void
MainWindow::mainModelChanged(WaveFileModel *model)
{
    SVDEBUG << "MainWindow::mainModelChanged(" << model << ")" << endl;

    m_salientPending.clear();
    m_salientCalculating = false;
    
    m_panLayer->setModel(model);

    MainWindowBase::mainModelChanged(model);

    if (m_playTarget || m_audioIO) {
        connect(m_mainLevelPan, SIGNAL(levelChanged(float)),
                this, SLOT(mainModelGainChanged(float)));
        connect(m_mainLevelPan, SIGNAL(panChanged(float)),
                this, SLOT(mainModelPanChanged(float)));
    }

    SVDEBUG << "Pane stack pane count = " << m_paneStack->getPaneCount() << endl;

    if (model && m_paneStack && (m_paneStack->getPaneCount() == 0)) {
	AddPaneCommand *command = new AddPaneCommand(this);
	CommandHistory::getInstance()->addCommand(command);
	Pane *pane = command->getPane();
	Layer *newLayer = m_document->createImportedLayer(model);
	if (newLayer) {
	    m_document->addLayerToView(pane, newLayer);
	}
        addSalientFeatureLayer(pane, model);
    } else {
        addSalientFeatureLayer(m_paneStack->getCurrentPane(), model);
    }

    m_document->setAutoAlignment(m_viewManager->getAlignMode());
}

void
MainWindow::mainModelGainChanged(float gain)
{
    if (m_playTarget) {
        m_playTarget->setOutputGain(gain);
    } else if (m_audioIO) {
        m_audioIO->setOutputGain(gain);
    }
}

void
MainWindow::mainModelPanChanged(float balance)
{
    // this is indeed stereo balance rather than pan
    if (m_playTarget) {
        m_playTarget->setOutputBalance(balance);
    } else if (m_audioIO) {
        m_audioIO->setOutputBalance(balance);
    }
}

void
MainWindow::modelGenerationFailed(QString transformName, QString message)
{
    if (message != "") {

        QMessageBox::warning
            (this,
             tr("Failed to generate layer"),
             tr("<b>Layer generation failed</b><p>Failed to generate derived layer.<p>The layer transform \"%1\" failed:<p>%2")
             .arg(transformName).arg(message),
             QMessageBox::Ok);
    } else {
        QMessageBox::warning
            (this,
             tr("Failed to generate layer"),
             tr("<b>Layer generation failed</b><p>Failed to generate a derived layer.<p>The layer transform \"%1\" failed.<p>No error information is available.")
             .arg(transformName),
             QMessageBox::Ok);
    }
}

void
MainWindow::modelGenerationWarning(QString /* transformName */, QString message)
{
    QMessageBox::warning
        (this, tr("Warning"), message, QMessageBox::Ok);
}

void
MainWindow::modelRegenerationFailed(QString layerName,
                                    QString transformName, QString message)
{
    if (message != "") {

        QMessageBox::warning
            (this,
             tr("Failed to regenerate layer"),
             tr("<b>Layer generation failed</b><p>Failed to regenerate derived layer \"%1\" using new data model as input.<p>The layer transform \"%2\" failed:<p>%3")
             .arg(layerName).arg(transformName).arg(message),
             QMessageBox::Ok);
    } else {
        QMessageBox::warning
            (this,
             tr("Failed to regenerate layer"),
             tr("<b>Layer generation failed</b><p>Failed to regenerate derived layer \"%1\" using new data model as input.<p>The layer transform \"%2\" failed.<p>No error information is available.")
             .arg(layerName).arg(transformName),
             QMessageBox::Ok);
    }
}

void
MainWindow::modelRegenerationWarning(QString layerName,
                                     QString /* transformName */,
                                     QString message)
{
    QMessageBox::warning
        (this, tr("Warning"), tr("<b>Warning when regenerating layer</b><p>When regenerating the derived layer \"%1\" using new data model as input:<p>%2").arg(layerName).arg(message), QMessageBox::Ok);
}

void
MainWindow::alignmentComplete(AlignmentModel *model)
{
    cerr << "MainWindow::alignmentComplete(" << model << ")" << endl;
    if (model) mapSalientFeatureLayer(model);
}

void
MainWindow::alignmentFailed(QString message)
{
    QMessageBox::warning
        (this,
         tr("Failed to calculate alignment"),
         tr("<b>Alignment calculation failed</b><p>Failed to calculate an audio alignment:<p>%1")
         .arg(message),
         QMessageBox::Ok);
}

void
MainWindow::rightButtonMenuRequested(Pane *pane, QPoint position)
{
//    cerr << "MainWindow::rightButtonMenuRequested(" << pane << ", " << position.x() << ", " << position.y() << ")" << endl;
    m_paneStack->setCurrentPane(pane);
    m_rightButtonMenu->popup(position);
}

void
MainWindow::showLayerTree()
{
    if (!m_layerTreeView.isNull()) {
        m_layerTreeView->show();
        m_layerTreeView->raise();
        return;
    }

    //!!! should use an actual dialog class
        
    m_layerTreeView = new QTreeView();
    LayerTreeModel *tree = new LayerTreeModel(m_paneStack);
    m_layerTreeView->resize(500, 300); //!!!
    m_layerTreeView->setModel(tree);
    m_layerTreeView->expandAll();
    m_layerTreeView->show();
}

void
MainWindow::handleOSCMessage(const OSCMessage & /* message */)
{
    cerr << "MainWindow::handleOSCMessage: Not implemented" << endl;
}

void
MainWindow::preferences()
{
    if (!m_preferencesDialog.isNull()) {
        m_preferencesDialog->show();
        m_preferencesDialog->raise();
        return;
    }

    m_preferencesDialog = new PreferencesDialog(this);

    // DeleteOnClose is safe here, because m_preferencesDialog is a
    // QPointer that will be zeroed when the dialog is deleted.  We
    // use it in preference to leaving the dialog lying around because
    // if you Cancel the dialog, it resets the preferences state
    // without resetting its own widgets, so its state will be
    // incorrect when next shown unless we construct it afresh
    m_preferencesDialog->setAttribute(Qt::WA_DeleteOnClose);

    m_preferencesDialog->show();
}

void
MainWindow::mouseEnteredWidget()
{
    QWidget *w = dynamic_cast<QWidget *>(sender());
    if (!w) return;

    if (w == m_mainLevelPan) {
        contextHelpChanged(tr("Adjust the master playback level"));
    } else if (w == m_playSpeed) {
        contextHelpChanged(tr("Adjust the master playback speed"));
    }
}

void
MainWindow::mouseLeftWidget()
{
    contextHelpChanged("");
}

void
MainWindow::website()
{
    openHelpUrl(tr("http://www.sonicvisualiser.org/sonicvector/"));
}

void
MainWindow::help()
{
    openHelpUrl(tr("http://www.sonicvisualiser.org/sonicvector/doc/"));
}

void
MainWindow::about()
{
    bool debug = false;
    QString version = "(unknown version)";

#ifdef BUILD_DEBUG
    debug = true;
#endif
#ifdef VECT_VERSION
#ifdef SVNREV
    version = tr("Release %1 : Revision %2").arg(VECT_VERSION).arg(SVNREV);
#else
    version = tr("Release %1").arg(VECT_VERSION);
#endif
#else
#ifdef SVNREV
    version = tr("Unreleased : Revision %1").arg(SVNREV);
#endif
#endif

    QString aboutText;

    aboutText += tr("<h3>About Sonic Vector</h3>");
    aboutText += tr("<p>Sonic Vector is a comparative viewer for sets of related audio recordings.</p>");
    aboutText += tr("<p>%1 : %2 configuration</p>")
        .arg(version)
        .arg(debug ? tr("Debug") : tr("Release"));

    aboutText += 
        "<p>Sonic Vector Copyright &copy; 2005 - 2014 Chris Cannam and<br>"
        "Queen Mary, University of London.</p>"
        "<p>This program uses library code from many other authors. Please<br>"
        "refer to the accompanying documentation for more information.</p>"
        "<p>This program is free software; you can redistribute it and/or<br>"
        "modify it under the terms of the GNU General Public License as<br>"
        "published by the Free Software Foundation; either version 2 of the<br>"
        "License, or (at your option) any later version.<br>See the file "
        "COPYING included with this distribution for more information.</p>";
    
    QMessageBox::about(this, tr("About Sonic Vector"), aboutText);
}

void
MainWindow::keyReference()
{
    m_keyReference->show();
}

void
MainWindow::loadStyle()
{
    m_viewManager->setGlobalDarkBackground(true);
    
    QString stylepath = ":vect.qss";
    QFile file(stylepath);
    if (!file.open(QFile::ReadOnly)) {
        std::cerr << "WARNING: Failed to open style file " << stylepath
                  << std::endl;
    } else {
        QString styleSheet = QLatin1String(file.readAll());
        qApp->setStyleSheet(styleSheet);
        QPalette pal(Qt::white, Qt::gray, Qt::white, Qt::black, Qt::gray, Qt::white, Qt::white, Qt::black, Qt::black);
        qApp->setPalette(pal);
    }
}



