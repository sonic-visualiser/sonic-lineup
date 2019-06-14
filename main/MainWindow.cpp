/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Vect
    An experimental audio player for plural recordings of a work
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2019 Chris Cannam and QMUL.
    
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
#include "base/TempDirectory.h"
#include "base/RecordDirectory.h"
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
#include "audio/AudioCallbackRecordTarget.h"
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
using std::pair;


MainWindow::MainWindow(SoundOptions options) :
    MainWindowBase(options),
    m_mainMenusCreated(false),
    m_playbackMenu(0),
    m_recentSessionsMenu(0),
    m_deleteSelectedAction(0),
    m_ffwdAction(0),
    m_rwdAction(0),
    m_recentSessions("RecentSessions", 20),
    m_exiting(false),
    m_preferencesDialog(0),
    m_layerTreeView(0),
    m_keyReference(new KeyReference()),
    m_displayMode(OutlineWaveformMode),
    m_salientCalculating(false),
    m_salientColour(0),
    m_sessionState(NoSession)
{
    setWindowTitle(QApplication::applicationName());

    UnitDatabase *udb = UnitDatabase::getInstance();
    udb->registerUnit("Hz");
    udb->registerUnit("dB");
    udb->registerUnit("s");

    ColourDatabase *cdb = ColourDatabase::getInstance();
    cdb->setUseDarkBackground(cdb->addColour(Qt::white, tr("White")), true);
    cdb->setUseDarkBackground(cdb->addColour(QColor(30, 150, 255), tr("Bright Blue")), true);
    cdb->setUseDarkBackground(cdb->addColour(Qt::red, tr("Bright Red")), true);
    cdb->setUseDarkBackground(cdb->addColour(Qt::green, tr("Bright Green")), true);
    cdb->setUseDarkBackground(cdb->addColour(QColor(255, 188, 80), tr("Bright Orange")), true);
    cdb->setUseDarkBackground(cdb->addColour(QColor(225, 74, 255), tr("Bright Purple")), true);
    cdb->setUseDarkBackground(cdb->addColour(Qt::yellow, tr("Bright Yellow")), true);

    Preferences::getInstance()->setResampleOnLoad(true);

    Preferences::getInstance()->setSpectrogramSmoothing
        (Preferences::SpectrogramInterpolated);

    Preferences::getInstance()->setSpectrogramXSmoothing
        (Preferences::SpectrogramXInterpolated);

    QSettings settings;

    settings.beginGroup("LayerDefaults");

    settings.setValue("spectrogram",
                      QString("<layer channel=\"-1\" windowSize=\"1024\" colourMap=\"White on Black\" windowHopLevel=\"2\"/>"));

    settings.setValue("melodicrange",
                      QString("<layer channel=\"-1\" gain=\"1\" normalizeVisibleArea=\"false\" columnNormalization=\"hybrid\" colourMap=\"Ice\" minFrequency=\"80\" maxFrequency=\"1500\" windowSize=\"8192\" windowOverlap=\"75\" binDisplay=\"0\" />"));

    settings.endGroup();

    settings.beginGroup("MainWindow");
    settings.setValue("showstatusbar", false);
    settings.endGroup();

    settings.beginGroup("IconLoader");
    settings.setValue("invert-icons-on-dark-background", true);
    settings.endGroup();

    settings.beginGroup("View");
    settings.setValue("showcancelbuttons", false);
    settings.endGroup();

    m_viewManager->setAlignMode(true);
    m_viewManager->setPlaySoloMode(true);
    m_viewManager->setToolMode(ViewManager::NavigateMode);
    m_viewManager->setZoomWheelsEnabled(false);
    m_viewManager->setIlluminateLocalFeatures(false);
    m_viewManager->setShowWorkTitle(true);
    m_viewManager->setOpportunisticEditingEnabled(false);

    loadStyle();
    
    QFrame *mainFrame = new QFrame;
    QGridLayout *mainLayout = new QGridLayout;

    setCentralWidget(mainFrame);
    
    m_mainScroll = new QScrollArea(mainFrame);
    m_mainScroll->setWidgetResizable(true);
    m_mainScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_mainScroll->setFrameShape(QFrame::NoFrame);

    m_paneStack->setResizeMode(PaneStack::AutoResizeOnly);
    m_paneStack->setLayoutStyle(PaneStack::NoPropertyStacks);
    m_paneStack->setShowCloseButtonOnFirstPane(false);
    m_paneStack->setShowAlignmentViews(true);
    m_mainScroll->setWidget(m_paneStack);

    QFrame *bottomFrame = new QFrame(mainFrame);
    bottomFrame->setObjectName("BottomFrame");
    QGridLayout *bottomLayout = new QGridLayout;

    int bottomElementHeight = m_viewManager->scalePixelSize(45);
    if (bottomElementHeight < 40) bottomElementHeight = 40;
    int bottomButtonHeight = (bottomElementHeight * 3) / 4;
    
    QButtonGroup *bg = new QButtonGroup;
    IconLoader il;

    QFrame *buttonFrame = new QFrame(bottomFrame);
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->setSpacing(0);
    buttonLayout->setMargin(0);
    buttonFrame->setLayout(buttonLayout);

    QPushButton *button = new QPushButton;
    button->setIcon(il.load("waveform"));
    button->setText(tr("Outline waveform"));
    button->setCheckable(true);
    button->setChecked(true);
    button->setFixedHeight(bottomButtonHeight);
    bg->addButton(button);
    buttonLayout->addWidget(button);
    connect(button, SIGNAL(clicked()), this, SLOT(outlineWaveformModeSelected()));
    m_modeButtons[OutlineWaveformMode] = button;
    m_modeLayerNames[OutlineWaveformMode] = "Outline Waveform"; // not to be translated

    button = new QPushButton;
    button->setIcon(il.load("waveform"));
    button->setText(tr("Waveform"));
    button->setCheckable(true);
    button->setChecked(false);
    button->setFixedHeight(bottomButtonHeight);
    bg->addButton(button);
    buttonLayout->addWidget(button);
    connect(button, SIGNAL(clicked()), this, SLOT(standardWaveformModeSelected()));
    m_modeButtons[WaveformMode] = button;
    m_modeLayerNames[WaveformMode] = "Waveform"; // not to be translated

    button = new QPushButton;
    button->setIcon(il.load("colour3d"));
    button->setText(tr("Melodic spectrogram"));
    button->setCheckable(true);
    button->setChecked(false);
    button->setFixedHeight(bottomButtonHeight);
    bg->addButton(button);
    buttonLayout->addWidget(button);
    connect(button, SIGNAL(clicked()), this, SLOT(melodogramModeSelected()));
    m_modeButtons[MelodogramMode] = button;
    m_modeLayerNames[MelodogramMode] = "Melodogram"; // not to be translated

    button = new QPushButton;
    button->setIcon(il.load("colour3d"));
    button->setText(tr("Spectrogram"));
    button->setCheckable(true);
    button->setChecked(false);
    button->setFixedHeight(bottomButtonHeight);
    bg->addButton(button);
    buttonLayout->addWidget(button);
    connect(button, SIGNAL(clicked()), this, SLOT(spectrogramModeSelected()));
    m_modeButtons[SpectrogramMode] = button;
    m_modeLayerNames[SpectrogramMode] = "Spectrogram"; // not to be translated

/*
    button = new QPushButton;
    button->setIcon(il.load("values"));
    button->setText(tr("Spectral flux"));
    button->setCheckable(true);
    button->setChecked(false);
    button->setFixedHeight(bottomButtonHeight);
    bg->addButton(button);
    buttonLayout->addWidget(button);
    connect(button, SIGNAL(clicked()), this, SLOT(curveModeSelected()));
    m_modeButtons[CurveMode] = button;
    m_modeLayerNames[CurveMode] = "Curve";
*/
    button = new QPushButton;
    button->setIcon(il.load("values"));
    button->setText(tr("Sung pitch"));
    button->setCheckable(true);
    button->setChecked(false);
    button->setFixedHeight(bottomButtonHeight);
    bg->addButton(button);
    buttonLayout->addWidget(button);
    connect(button, SIGNAL(clicked()), this, SLOT(pitchModeSelected()));
    m_modeButtons[PitchMode] = button;
    m_modeLayerNames[PitchMode] = "Pitch"; // not to be translated

    button = new QPushButton;
    button->setIcon(il.load("colour3d"));
    button->setText(tr("Key"));
    button->setCheckable(true);
    button->setChecked(false);
    button->setFixedHeight(bottomButtonHeight);
    bg->addButton(button);
    buttonLayout->addWidget(button);
    connect(button, SIGNAL(clicked()), this, SLOT(keyModeSelected()));
    m_modeButtons[KeyMode] = button;
    m_modeLayerNames[KeyMode] = "Key"; // not to be translated

    button = new QPushButton;
    button->setIcon(il.load("colour3d"));
    button->setText(tr("Stereo azimuth"));
    button->setCheckable(true);
    button->setChecked(false);
    button->setFixedHeight(bottomButtonHeight);
    bg->addButton(button);
    buttonLayout->addWidget(button);
    connect(button, SIGNAL(clicked()), this, SLOT(azimuthModeSelected()));
    m_modeButtons[AzimuthMode] = button;
    m_modeLayerNames[AzimuthMode] = "Azimuth"; // not to be translated

    m_playSpeed = new AudioDial(bottomFrame);
    m_playSpeed->setMinimum(0);
    m_playSpeed->setMaximum(120);
    m_playSpeed->setValue(60);
    m_playSpeed->setFixedWidth(bottomElementHeight);
    m_playSpeed->setFixedHeight(bottomElementHeight);
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

    m_mainLevelPan = new LevelPanToolButton(bottomFrame);
    connect(m_mainLevelPan, SIGNAL(mouseEntered()), this, SLOT(mouseEnteredWidget()));
    connect(m_mainLevelPan, SIGNAL(mouseLeft()), this, SLOT(mouseLeftWidget()));
    m_mainLevelPan->setFixedHeight(bottomElementHeight);
    m_mainLevelPan->setFixedWidth(bottomElementHeight);
    m_mainLevelPan->setImageSize((bottomElementHeight * 3) / 4);
    m_mainLevelPan->setBigImageSize(bottomElementHeight * 3);

    int spacing = m_viewManager->scalePixelSize(4);

    bottomLayout->setSpacing(spacing);
    bottomLayout->setMargin(spacing);
    bottomLayout->addWidget(buttonFrame, 0, 0);
    bottomLayout->setColumnStretch(1, 10);
    bottomLayout->addWidget(m_playSpeed, 0, 2);
    bottomLayout->addWidget(m_mainLevelPan, 0, 3);
    bottomFrame->setLayout(bottomLayout);
    
    mainLayout->setSpacing(spacing);
    mainLayout->setMargin(spacing);
    mainLayout->addWidget(m_mainScroll, 0, 0);
    mainLayout->addWidget(bottomFrame, 1, 0);
    mainFrame->setLayout(mainLayout);

    setupMenus();
    setupToolbars();
    setupHelpMenu();

    statusBar()->hide();

    setIconsVisibleInMenus(false);
    finaliseMenus();

    openMostRecentSession();
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
    }

    setupFileMenu();
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
        m_scrollLeftAction, m_scrollRightAction
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
    action = new QAction(icon, tr("&Add Files..."), this);
    action->setShortcut(tr("Ctrl+O"));
    action->setStatusTip(tr("Add one or more audio files"));
    connect(action, SIGNAL(triggered()), this, SLOT(openFiles()));
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

    m_recentSessionsMenu = menu->addMenu(tr("&Recent Sessions"));
    m_recentSessionsMenu->setTearOffEnabled(false);
    setupRecentSessionsMenu();
    connect(&m_recentSessions, SIGNAL(recentChanged()),
            this, SLOT(setupRecentSessionsMenu()));

    menu->addSeparator();
    
    action = new QAction(tr("Browse Recorded Audio"), this);
    action->setStatusTip(tr("Open the Recorded Audio folder in the system file browser"));
    connect(action, SIGNAL(triggered()), this, SLOT(browseRecordedAudio()));
    menu->addAction(action);
    
    /*
    menu->addSeparator();
    action = new QAction(tr("&Preferences..."), this);
    action->setStatusTip(tr("Adjust the application preferences"));
    connect(action, SIGNAL(triggered()), this, SLOT(preferences()));
    menu->addAction(action);
    */
    
    menu->addSeparator();
    action = new QAction(il.load("exit"), tr("&Quit"), this);
    action->setShortcut(tr("Ctrl+Q"));
    action->setStatusTip(tr("Exit Sonic Vector"));
    connect(action, SIGNAL(triggered()), this, SLOT(close()));
    m_keyReference->registerShortcut(action);
    menu->addAction(action);
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

    action = new QAction(tr("Show Salient Feature Layers"), this);
    action->setShortcut(tr("#"));
    action->setStatusTip(tr("Show or hide all salient-feature layers"));
    connect(action, SIGNAL(triggered()), this, SLOT(toggleSalientFeatures()));
    action->setCheckable(true);
    action->setChecked(true);
    m_keyReference->registerShortcut(action);
    menu->addAction(action);

    action = new QAction(tr("Show Vertical Scales"), this);
    action->setShortcut(tr("S"));
    action->setStatusTip(tr("Show or hide all vertical scales"));
    connect(action, SIGNAL(triggered()), this, SLOT(toggleVerticalScales()));
    action->setCheckable(true);
    action->setChecked(false);
    m_keyReference->registerShortcut(action);
    menu->addAction(action);
        
#ifndef Q_OS_MAC
    menu->addSeparator();

    // Only on non-Mac platforms -- on the Mac this interacts very
    // badly with the "native" full-screen mode
    action = new QAction(tr("Go Full-Screen"), this);
    action->setShortcut(tr("F11"));
    action->setStatusTip(tr("Expand the pane area to the whole screen"));
    connect(action, SIGNAL(triggered()), this, SLOT(goFullScreen()));
    m_keyReference->registerShortcut(action);
    menu->addAction(action);
#endif
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
MainWindow::setupRecentSessionsMenu()
{
    m_recentSessionsMenu->clear();
    vector<pair<QString, QString>> sessions = m_recentSessions.getRecentEntries();
    for (size_t i = 0; i < sessions.size(); ++i) {
        QString path = sessions[i].first;
        QString label = sessions[i].second;
        if (label == "") label = path;
        QAction *action = new QAction(label, this);
        action->setObjectName(path);
	connect(action, SIGNAL(triggered()), this, SLOT(openRecentSession()));
        if (i == 0) {
            action->setShortcut(tr("Ctrl+R"));
            m_keyReference->registerShortcut
                (tr("Re-open"),
                 action->shortcut().toString(),
                 tr("Re-open the current or most recently opened session"));
        }
	m_recentSessionsMenu->addAction(action);
    }
}

void
MainWindow::setupToolbars()
{
    m_keyReference->setCategory(tr("Playback and Transport Controls"));

    IconLoader il;

    QMenu *menu = m_playbackMenu = menuBar()->addMenu(tr("Play&back"));
    menu->setTearOffEnabled(false);

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

    QAction *recordAction = toolbar->addAction(il.load("record"),
                                               tr("Record"));
    recordAction->setCheckable(true);
    recordAction->setShortcut(tr("Ctrl+Space"));
    recordAction->setStatusTip(tr("Record a new audio file"));
    connect(recordAction, SIGNAL(triggered()), this, SLOT(record()));
    connect(m_recordTarget, SIGNAL(recordStatusChanged(bool)),
            recordAction, SLOT(setChecked(bool)));
    connect(this, SIGNAL(canRecord(bool)),
            recordAction, SLOT(setEnabled(bool)));

    QAction *alAction = 0;
    alAction = toolbar->addAction(il.load("align"),
                                  tr("Align File Timelines"));
    alAction->setCheckable(true);
    alAction->setChecked(m_viewManager->getAlignMode());
    alAction->setStatusTip(tr("Treat multiple audio files as versions of the same work, and align their timelines"));
    connect(m_viewManager, SIGNAL(alignModeChanged(bool)),
            alAction, SLOT(setChecked(bool)));
    connect(alAction, SIGNAL(triggered()), this, SLOT(alignToggled()));

    QSettings settings;

    QAction *tdAction = 0;
    tdAction = new QAction(tr("Allow for Pitch Difference when Aligning"), this);
    tdAction->setCheckable(true);
    settings.beginGroup("Alignment");
    tdAction->setChecked(settings.value("align-pitch-aware", true).toBool());
    settings.endGroup();
    tdAction->setStatusTip(tr("Compare relative pitch content of audio files before aligning, in order to correctly align recordings of the same material at different tuning pitches"));
    connect(tdAction, SIGNAL(triggered()), this, SLOT(tuningDifferenceToggled()));

    m_keyReference->registerShortcut(m_playAction);
    m_keyReference->registerShortcut(m_rwdAction);
    m_keyReference->registerShortcut(m_ffwdAction);
    m_keyReference->registerShortcut(rwdStartAction);
    m_keyReference->registerShortcut(ffwdEndAction);
    m_keyReference->registerShortcut(recordAction);

    menu->addAction(m_playAction);
    menu->addSeparator();
    menu->addAction(m_rwdAction);
    menu->addAction(m_ffwdAction);
    menu->addSeparator();
    menu->addAction(rwdStartAction);
    menu->addAction(ffwdEndAction);
    menu->addSeparator();
    menu->addAction(alAction);
    menu->addAction(tdAction);
    menu->addSeparator();
    menu->addAction(recordAction);
    menu->addSeparator();

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
    // we don't actually have a description label
}

void
MainWindow::updateWindowTitle()
{
    QString title;

    QString sessionLabel = makeSessionLabel();
    
    if (sessionLabel != "") {
        title = tr("%1: %2")
            .arg(QApplication::applicationName())
            .arg(sessionLabel);
    } else {
        title = QApplication::applicationName();
    }
    
    setWindowTitle(title);
}

void
MainWindow::documentModified()
{
    MainWindowBase::documentModified();
}

void
MainWindow::documentRestored()
{
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
MainWindow::browseRecordedAudio()
{
    QString path = RecordDirectory::getRecordContainerDirectory();
    if (path == "") path = RecordDirectory::getRecordDirectory();
    if (path == "") return;

    openLocalFolder(path);
}

void
MainWindow::newSession()
{
    cerr << "MainWindow::newSession" << endl;

    closeSession();
    createDocument();

    // Reset to a waveform mode; this is because it takes less time to
    // process & render than other modes, so we will be able to
    // checkpoint sooner - the result of starting out in e.g. pitch
    // mode can be quite strange because of the near-eternity before a
    // safe checkpoint can be made
    
    m_displayMode = OutlineWaveformMode;
    for (auto &bp : m_modeButtons) {
        bp.second->setChecked(false);
    }
    m_modeButtons[m_displayMode]->setChecked(true);
    
    // We need a pane, so that we have something to receive drop events
    
    Pane *pane = m_paneStack->addPane();

    connect(pane, SIGNAL(contextHelpChanged(const QString &)),
            this, SLOT(contextHelpChanged(const QString &)));

    m_document->setAutoAlignment(m_viewManager->getAlignMode());

    CommandHistory::getInstance()->clear();
    CommandHistory::getInstance()->documentSaved();
    documentRestored();
    updateMenuStates();
}

void
MainWindow::closeSession()
{
    checkpointSession();
    if (m_sessionState != SessionLoading) {
        m_sessionFile = "";
        m_sessionState = NoSession;
    }

    while (m_paneStack->getPaneCount() > 0) {

	Pane *pane = m_paneStack->getPane(m_paneStack->getPaneCount() - 1);

	while (pane->getLayerCount() > 0) {
	    m_document->removeLayerFromView
		(pane, pane->getLayer(pane->getLayerCount() - 1));
	}

	m_paneStack->deletePane(pane);
    }

    while (m_paneStack->getHiddenPaneCount() > 0) {

	Pane *pane = m_paneStack->getHiddenPane
	    (m_paneStack->getHiddenPaneCount() - 1);

	while (pane->getLayerCount() > 0) {
	    m_document->removeLayerFromView
		(pane, pane->getLayer(pane->getLayerCount() - 1));
	}

	m_paneStack->deletePane(pane);
    }

    delete m_document;
    m_document = 0;
    m_viewManager->clearSelections();
    m_timeRulerLayer = 0; // document owned this

    setWindowTitle(tr("Sonic Vector"));

    CommandHistory::getInstance()->clear();
    CommandHistory::getInstance()->documentSaved();
    documentRestored();
}

void
MainWindow::openFiles()
{
    QString orig = m_audioFile;
    if (orig == "") orig = ".";
    else orig = QFileInfo(orig).absoluteDir().canonicalPath();

    FileFinder *ff = FileFinder::getInstance();

    QStringList paths = ff->getOpenFileNames(FileFinder::AudioFile,
                                             m_audioFile);

    m_sessionState = SessionActive;
    
    for (QString path: paths) {
        
        FileOpenStatus status = openPath(path, CreateAdditionalModel);

        if (status != FileOpenSucceeded) {
            QMessageBox::critical(this, tr("Failed to open file"),
                                  tr("<b>File open failed</b><p>File \"%1\" could not be opened").arg(path));
        } else {
            configureNewPane(m_paneStack->getCurrentPane());
        }
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

    m_sessionState = SessionActive;
    
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
MainWindow::openRecentSession()
{
    QObject *obj = sender();
    QAction *action = dynamic_cast<QAction *>(obj);
    
    if (!action) {
	cerr << "WARNING: MainWindow::openRecentSession: sender is not an action"
		  << endl;
	return;
    }

    QString path = action->objectName();
    
    if (path == "") {
        cerr << "WARNING: MainWindow::openRecentSession: action incorrectly named"
             << endl;
        return;
    }

    openSmallSessionFile(path);
}

void
MainWindow::openMostRecentSession()
{
    vector<QString> files = m_recentSessions.getRecentIdentifiers();
    if (files.empty()) return;

    QString path = files[0];
    if (path == "") return;

    openSmallSessionFile(path);
}

void
MainWindow::openSmallSessionFile(QString path)
{
    m_sessionFile = path;
    m_sessionState = SessionLoading;

    SVDEBUG << "MainWindow::openSmallSessionFile: m_sessionFile is now "
            << m_sessionFile << endl;

    SmallSession session;
    FileOpenStatus status;
    QString errorText;
    
    try {
        session = SmallSession::load(path);
    } catch (const std::runtime_error &e) {
        errorText = e.what();
        goto failed;
    }

    closeSession();
    createDocument();

    status = openPath(session.mainFile, ReplaceMainModel);

    if (status != FileOpenSucceeded) {
        errorText = tr("Unable to open main audio file %1")
            .arg(session.mainFile);
        goto failed;
    }
    
    configureNewPane(m_paneStack->getCurrentPane());
        
    for (QString path: session.additionalFiles) {
        
        status = openPath(path, CreateAdditionalModel);

        if (status != FileOpenSucceeded) {
            errorText = tr("Unable to open audio file %1").arg(path);
            goto failed;
        }

        configureNewPane(m_paneStack->getCurrentPane());
    }

    m_documentModified = false;
    m_sessionState = SessionActive;
    return;

failed:
    QMessageBox::critical(this, tr("Failed to reload session"),
                          tr("<b>Open failed</b><p>Session file \"%1\" could not be opened: %2</p>").arg(path).arg(errorText));
    m_sessionFile = "";
    m_sessionState = NoSession;
}

bool
MainWindow::selectExistingLayerForMode(Pane *pane,
                                       QString modeName,
                                       Model **createFrom)
{
    // Search the given pane for any layer whose object name matches
    // modeName, showing it if it exists, and hiding all other layers
    // (except for time-instants layers, which are assumed to be used
    // for persistent segment display and are left unmodified).

    // In the case where no such layer is found and false is returned,
    // then if the return parameter createFrom is non-null, the value
    // it points to will be set to a pointer to the model from which
    // such a layer should be constructed.

    Model *model = 0;

    bool have = false;

    for (int i = 0; i < pane->getLayerCount(); ++i) {
        
        Layer *layer = pane->getLayer(i);
        if (!layer || qobject_cast<TimeInstantLayer *>(layer)) {
            continue;
        }
        
        Model *lm = layer->getModel();
        while (lm && lm->getSourceModel()) lm = lm->getSourceModel();
        if (qobject_cast<WaveFileModel *>(lm)) model = lm;
        
        QString ln = layer->objectName();

        if (ln == modeName) {
            layer->showLayer(pane, true);
            have = true;
        } else {
            layer->showLayer(pane, false);
        }
    }
    
    if (have) return true;

    if (createFrom) {
        *createFrom = model;
    }
    return false;
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

    ModelTransformer::Input input(model, -1);

    Layer *newLayer = m_document->createDerivedLayer(transform, model);

    if (newLayer) {

        TimeInstantLayer *til = qobject_cast<TimeInstantLayer *>(newLayer);
        if (til) {
            til->setPlotStyle(TimeInstantLayer::PlotInstants);
            til->setBaseColour(m_salientColour);
        }

        PlayParameters *params = newLayer->getPlayParameters();
        if (params) {
            params->setPlayAudible(false);
        }

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
    if (layer && layer->getCompletion(0) == 100) {
        m_salientCalculating = false;
        foreach (AlignmentModel *am, m_salientPending) {
            mapSalientFeatureLayer(am);
        }
        m_salientPending.clear();
    }
}

TimeInstantLayer *
MainWindow::findSalientFeatureLayer(Pane *pane)
{
    if (!getMainModel()) return nullptr;
    
    if (!pane) {
        for (int i = 0; i < m_paneStack->getPaneCount(); ++i) {

            Pane *p = m_paneStack->getPane(i);
            bool isAssociatedWithMainModel = false;

            for (int j = 0; j < p->getLayerCount(); ++j) {
                Layer *l = p->getLayer(j);
                if (l->getModel() == getMainModel()) {
                    isAssociatedWithMainModel = true;
                    break;
                }
            }

            if (isAssociatedWithMainModel) {
                TimeInstantLayer *layerHere = findSalientFeatureLayer(p);
                if (layerHere) return layerHere;
            }
        }

        return nullptr;
    }

    for (int i = 0; i < pane->getLayerCount(); ++i) {
        TimeInstantLayer *ll = qobject_cast<TimeInstantLayer *>
            (pane->getLayer(i));
        if (ll) return ll;
    }

    return nullptr;
}

void
MainWindow::toggleVerticalScales()
{
    if (m_viewManager->getOverlayMode() == ViewManager::NoOverlays) {
        m_viewManager->setOverlayMode(ViewManager::StandardOverlays);
    } else {
        m_viewManager->setOverlayMode(ViewManager::NoOverlays);
    }
}

void
MainWindow::toggleSalientFeatures()
{
    bool targetDormantState = false;

    for (int i = 0; i < m_paneStack->getPaneCount(); ++i) {
        Pane *p = m_paneStack->getPane(i);
        TimeInstantLayer *layer = findSalientFeatureLayer(p);
        if (layer) {
            targetDormantState = !(layer->isLayerDormant(p));
            break;
        }
    }

    for (int i = 0; i < m_paneStack->getPaneCount(); ++i) {
        Pane *p = m_paneStack->getPane(i);
        TimeInstantLayer *salient = findSalientFeatureLayer(p);
        if (salient) {
            salient->setLayerDormant(p, targetDormantState);
        }
        if (targetDormantState) {
            for (int j = 0; j < p->getLayerCount(); ++j) {
                Layer *l = p->getLayer(j);
                if (l != salient) {
                    p->propertyContainerSelected(p, l);
                    break;
                }
            }
        } else {
            p->propertyContainerSelected(p, salient);
        }
    }
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
        SVCERR << "MainWindow::mapSalientFeatureLayer: No salient layer found"
               << endl;
        m_salientPending.insert(am);
        return;
    }

    if (!am) {
        SVCERR << "MainWindow::mapSalientFeatureLayer: AlignmentModel is null!"
               << endl;
        return;
    }
    
    const Model *model = am->getAlignedModel();

    Pane *pane = nullptr;
    Layer *layer = nullptr;
    Pane *firstPane = nullptr;
    
    for (int i = 0; i < m_paneStack->getPaneCount(); ++i) {
        Pane *p = m_paneStack->getPane(i);
        if (p && !firstPane) firstPane = p;
        for (int j = 0; j < p->getLayerCount(); ++j) {
            Layer *l = p->getLayer(j);
            if (!l) continue;
            if (l->getModel() == model) {
                pane = p;
                layer = l;
                break;
            }
        }
        if (layer) break;
    }

    if (!pane || !layer) {
        SVCERR << "MainWindow::mapSalientFeatureLayer: Failed to find model "
               << model << " in any layer" << endl;
        return;
    }

    QString salientLayerName = tr("Mapped Salient Feature Layer");

    // Remove any existing mapped salient layer from this pane (in
    // case we are re-aligning an existing model)
    for (int j = 0; j < pane->getLayerCount(); ++j) {
        Layer *l = pane->getLayer(j);
        if (!l) continue;
        if (l->objectName() == salientLayerName) {
            SVDEBUG << "MainWindow::mapSalientFeatureLayer: "
                    << "Removing existing mapped layer " << l << endl;
            m_document->deleteLayer(l, true); // force flag: remove from views
            break;
        }
    }

    pane->setCentreFrame(am->fromReference(firstPane->getCentreFrame()));
    
    const SparseOneDimensionalModel *from =
        qobject_cast<const SparseOneDimensionalModel *>(salient->getModel());
    if (!from) {
        SVCERR << "MainWindow::mapSalientFeatureLayer: "
               << "Salient layer lacks SparseOneDimensionalModel" << endl;
        return;
    }
        
    SparseOneDimensionalModel *to = new SparseOneDimensionalModel
        (model->getSampleRate(), from->getResolution(), false);

    EventVector pp = from->getAllEvents();
    for (const auto &p: pp) {
        Event aligned = p
            .withFrame(am->fromReference(p.getFrame()))
            .withLabel(""); // remove label, as the analysis was not
                            // conducted on the audio we're mapping to
        to->add(aligned);
    }

    Layer *newLayer = m_document->createImportedLayer(to);

    if (newLayer) {

        newLayer->setObjectName(salientLayerName);
        
        TimeInstantLayer *til = qobject_cast<TimeInstantLayer *>(newLayer);
        if (til) {
            til->setPlotStyle(TimeInstantLayer::PlotInstants);
            til->setBaseColour(m_salientColour);
        }
        
        PlayParameters *params = newLayer->getPlayParameters();
        if (params) {
            params->setPlayAudible(false);
        }

        m_document->addLayerToView(pane, newLayer);
        m_paneStack->setCurrentLayer(pane, newLayer);
    }
}

void
MainWindow::outlineWaveformModeSelected()
{
    QString name = m_modeLayerNames[OutlineWaveformMode];

    Pane *currentPane = m_paneStack->getCurrentPane();
    
    for (int i = 0; i < m_paneStack->getPaneCount(); ++i) {

        Pane *pane = m_paneStack->getPane(i);
        if (!pane) continue;

        Model *createFrom = nullptr;
        if (!selectExistingLayerForMode(pane, name, &createFrom) &&
            createFrom) {

            Layer *newLayer = m_document->createLayer(LayerFactory::Waveform);
            newLayer->setObjectName(name);

            QString layerPropertyXml =
                QString("<layer scale=\"%1\" channelMode=\"%2\" gain=\"0.95\"/>")
                .arg(int(WaveformLayer::MeterScale))
                .arg(int(WaveformLayer::MergeChannels));
            LayerFactory::getInstance()->setLayerProperties
                (newLayer, layerPropertyXml);

            SingleColourLayer *scl =
                qobject_cast<SingleColourLayer *>(newLayer);
            if (scl) {
                scl->setBaseColour
                    (i % ColourDatabase::getInstance()->getColourCount());
            }
            
            m_document->setModel(newLayer, createFrom);
            m_document->addLayerToView(pane, newLayer);
            m_paneStack->setCurrentLayer(pane, newLayer);
        }

        TimeInstantLayer *salient = findSalientFeatureLayer(pane);
        if (salient) {
            pane->propertyContainerSelected(pane, salient);
        }
    }

    if (currentPane) {
        m_paneStack->setCurrentPane(currentPane);
    }
    
    m_displayMode = OutlineWaveformMode;
    checkpointSession();
}

void
MainWindow::standardWaveformModeSelected()
{
    QString name = m_modeLayerNames[WaveformMode];

    Pane *currentPane = m_paneStack->getCurrentPane();
    
    for (int i = 0; i < m_paneStack->getPaneCount(); ++i) {

        Pane *pane = m_paneStack->getPane(i);
        if (!pane) continue;

        Model *createFrom = nullptr;
        if (!selectExistingLayerForMode(pane, name, &createFrom) &&
            createFrom) {

            Layer *newLayer = m_document->createLayer(LayerFactory::Waveform);
            newLayer->setObjectName(name);

            QString layerPropertyXml =
                QString("<layer scale=\"%1\" channelMode=\"%2\"/>")
                .arg(int(WaveformLayer::LinearScale))
                .arg(int(WaveformLayer::SeparateChannels));
            LayerFactory::getInstance()->setLayerProperties
                (newLayer, layerPropertyXml);

            SingleColourLayer *scl =
                qobject_cast<SingleColourLayer *>(newLayer);
            if (scl) {
                scl->setBaseColour
                    (i % ColourDatabase::getInstance()->getColourCount());
            }
            
            m_document->setModel(newLayer, createFrom);
            m_document->addLayerToView(pane, newLayer);
            m_paneStack->setCurrentLayer(pane, newLayer);
        }

        TimeInstantLayer *salient = findSalientFeatureLayer(pane);
        if (salient) {
            pane->propertyContainerSelected(pane, salient);
        }
    }

    if (currentPane) {
        m_paneStack->setCurrentPane(currentPane);
    }

    m_displayMode = WaveformMode;
    checkpointSession();
}

void
MainWindow::spectrogramModeSelected()
{
    QString name = m_modeLayerNames[SpectrogramMode];

    Pane *currentPane = m_paneStack->getCurrentPane();
    
    for (int i = 0; i < m_paneStack->getPaneCount(); ++i) {

        Pane *pane = m_paneStack->getPane(i);
        if (!pane) continue;

        Model *createFrom = nullptr;
        if (!selectExistingLayerForMode(pane, name, &createFrom) &&
            createFrom) {
            Layer *newLayer = m_document->createLayer(LayerFactory::Spectrogram);
            newLayer->setObjectName(name);
            m_document->setModel(newLayer, createFrom);
            m_document->addLayerToView(pane, newLayer);
            m_paneStack->setCurrentLayer(pane, newLayer);
        }

        TimeInstantLayer *salient = findSalientFeatureLayer(pane);
        if (salient) {
            pane->propertyContainerSelected(pane, salient);
        }
    }

    if (currentPane) {
        m_paneStack->setCurrentPane(currentPane);
    }

    m_displayMode = SpectrogramMode;
    checkpointSession();
}

void
MainWindow::melodogramModeSelected()
{
    QString name = m_modeLayerNames[MelodogramMode];

    Pane *currentPane = m_paneStack->getCurrentPane();

    for (int i = 0; i < m_paneStack->getPaneCount(); ++i) {

        Pane *pane = m_paneStack->getPane(i);
        if (!pane) continue;

        Model *createFrom = nullptr;
        if (!selectExistingLayerForMode(pane, name, &createFrom) &&
            createFrom) {
            Layer *newLayer = m_document->createLayer
                (LayerFactory::MelodicRangeSpectrogram);
            newLayer->setObjectName(name);
            m_document->setModel(newLayer, createFrom);
            m_document->addLayerToView(pane, newLayer);
            m_paneStack->setCurrentLayer(pane, newLayer);
        }

        TimeInstantLayer *salient = findSalientFeatureLayer(pane);
        if (salient) {
            pane->propertyContainerSelected(pane, salient);
        }
    }

    if (currentPane) {
        m_paneStack->setCurrentPane(currentPane);
    }

    m_displayMode = MelodogramMode;
    checkpointSession();
}

void
MainWindow::selectTransformDrivenMode(DisplayMode mode,
                                      QString transformId,
                                      QString layerPropertyXml)
{
    QString name = m_modeLayerNames[mode];
    
    Pane *currentPane = m_paneStack->getCurrentPane();

    for (int i = 0; i < m_paneStack->getPaneCount(); ++i) {

        Pane *pane = m_paneStack->getPane(i);
        if (!pane) continue;

        Model *createFrom = nullptr;
        if (!selectExistingLayerForMode(pane, name, &createFrom) &&
            createFrom) {

            TransformFactory *tf = TransformFactory::getInstance();

            if (tf->haveTransform(transformId)) {

                Transform transform = tf->getDefaultTransformFor
                    (transformId, createFrom->getSampleRate());

                ModelTransformer::Input input(createFrom, -1);
                
                Layer *newLayer =
                    m_document->createDerivedLayer(transform, createFrom);

                if (newLayer) {
                    newLayer->setObjectName(name);
                    LayerFactory::getInstance()->setLayerProperties
                        (newLayer, layerPropertyXml);

                    SingleColourLayer *scl =
                        qobject_cast<SingleColourLayer *>(newLayer);
                    if (scl) {
                        scl->setBaseColour
                            (i % ColourDatabase::getInstance()->getColourCount());
                    }

                    m_document->addLayerToView(pane, newLayer);
                    m_paneStack->setCurrentLayer(pane, newLayer);
                } else {
                    SVCERR << "ERROR: Failed to create derived layer" << endl;
                }
            
            } else {
                SVCERR << "ERROR: No PYin plugin available" << endl;
            }
        }

        TimeInstantLayer *salient = findSalientFeatureLayer(pane);
        if (salient) {
            pane->propertyContainerSelected(pane, salient);
        }
    }

    if (currentPane) {
        m_paneStack->setCurrentPane(currentPane);
    }

    m_displayMode = mode;
    checkpointSession();
}

void
MainWindow::curveModeSelected()
{
    QString propertyXml =
        QString("<layer plotStyle=\"%1\"/>")
        .arg(int(TimeValueLayer::PlotStems));

    selectTransformDrivenMode
        (CurveMode,
         "vamp:qm-vamp-plugins:qm-onsetdetector:detection_fn",
         propertyXml);
}

void
MainWindow::pitchModeSelected()
{
    QString propertyXml =
        QString("<layer plotStyle=\"%1\" verticalScale=\"%2\"/>")
        .arg(int(TimeValueLayer::PlotDiscreteCurves))
        .arg(int(TimeValueLayer::LogScale));
    
    selectTransformDrivenMode
        (PitchMode,
         "vamp:pyin:pyin:smoothedpitchtrack",
         propertyXml);
}

void
MainWindow::keyModeSelected()
{
    QString propertyXml =
        QString("<layer colourMap=\"Sunset\" opaque=\"true\" smooth=\"false\" "
                "binScale=\"%1\" columnNormalization=\"none\"/>")
        .arg(int(BinScale::Linear));
    
    selectTransformDrivenMode
        (KeyMode,
         "vamp:qm-vamp-plugins:qm-keydetector:keystrength",
         propertyXml);
}

void
MainWindow::azimuthModeSelected()
{
    QString propertyXml =
        QString("<layer colourMap=\"Ice\" opaque=\"true\" smooth=\"true\" "
                "binScale=\"%1\" columnNormalization=\"hybrid\"/>")
        .arg(int(BinScale::Linear));

    selectTransformDrivenMode
        (AzimuthMode,
         "vamp:azi:azi:plan",
         propertyXml);
}

void
MainWindow::updateModeFromLayers()
{
    for (auto &bp : m_modeButtons) {
        bp.second->setChecked(false);
    }

    SVCERR << "MainWindow::updateModeFromLayers" << endl;
    
    for (int i = 0; i < m_paneStack->getPaneCount(); ++i) {

        Pane *pane = m_paneStack->getPane(i);
        if (!pane) continue;

        SVCERR << "MainWindow::updateModeFromLayers: pane " << i << "..." << endl;
        
        bool found = false;
        
        for (int j = 0; j < pane->getLayerCount(); ++j) {

            Layer *layer = pane->getLayer(j);
            if (!layer || qobject_cast<TimeInstantLayer *>(layer)) {
                continue;
            }
            if (layer->isLayerDormant(pane)) {
                continue;
            }

            QString ln = layer->objectName();

            SVCERR << "MainWindow::updateModeFromLayers: layer " << j << " has name " << ln << endl;

            for (const auto &mp: m_modeLayerNames) {
                if (ln == mp.second) {
                    m_displayMode = mp.first;
                    found = true;
                    break;
                }
            }
        }

        if (found) break;
    }

    m_modeButtons[m_displayMode]->setChecked(true);
}

void
MainWindow::reselectMode()
{
    switch (m_displayMode) {
    case CurveMode: curveModeSelected(); break;
    case OutlineWaveformMode: outlineWaveformModeSelected(); break;
    case WaveformMode: standardWaveformModeSelected(); break;
    case SpectrogramMode: spectrogramModeSelected(); break;
    case MelodogramMode: melodogramModeSelected(); break;
    case AzimuthMode: azimuthModeSelected(); break;
    case PitchMode: pitchModeSelected(); break;
    case KeyMode: keyModeSelected(); break;
    }
}

void
MainWindow::paneAdded(Pane *pane)
{
    pane->setPlaybackFollow(PlaybackScrollContinuous);
    m_paneStack->sizePanesEqually();
    checkpointSession();
}    

void
MainWindow::paneHidden(Pane *)
{
    checkpointSession();
}    

void
MainWindow::paneAboutToBeDeleted(Pane *)
{
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
    SVCERR << "MainWindow::configureNewPane(" << pane << ")" << endl;

    if (!pane) return;

    zoomToFit();
    reselectMode();

    Layer *waveformLayer = 0;

    for (int i = 0; i < pane->getLayerCount(); ++i) {
        Layer *layer = pane->getLayer(i);
        if (!layer) continue;
        if (dynamic_cast<WaveformLayer *>(layer)) waveformLayer = layer;
        if (dynamic_cast<TimeValueLayer *>(layer)) return;
    }
    if (!waveformLayer) return;

    waveformLayer->setObjectName(tr("Waveform"));
}

void
MainWindow::record()
{
    MainWindowBase::record();
    configureNewPane(m_paneStack->getCurrentPane());
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

    closeSession();

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

    e->accept();

    m_exiting = true;
    qApp->closeAllWindows();
    
    return;
}

bool
MainWindow::checkSaveModified()
{
    // It should always be OK to save, with our active-session paradigm
    return true;
}

bool
MainWindow::commitData(bool /* mayAskUser */)
{
    if (m_preferencesDialog &&
        m_preferencesDialog->isVisible()) {
        m_preferencesDialog->applicationClosing(true);
    }
    checkpointSession();
    return true;
}

void
MainWindow::preferenceChanged(PropertyContainer::PropertyName name)
{
    MainWindowBase::preferenceChanged(name);
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
MainWindow::tuningDifferenceToggled()
{
    QSettings settings;
    settings.beginGroup("Alignment");
    bool on = settings.value("align-pitch-aware", false).toBool();
    settings.setValue("align-pitch-aware", !on);
    settings.endGroup();

    if (m_viewManager->getAlignMode()) {
        m_document->realignModels();
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

QString
MainWindow::makeSessionFilename()
{
    Model *mainModel = getMainModel();
    if (!mainModel) {
        SVDEBUG << "MainWindow::makeSessionFilename: No main model, returning empty filename" << endl;
        return {};
    }
    
    //!!! can refactor in common with RecordDirectory?
    
    QDir parentDir(TempDirectory::getInstance()->getContainingPath());
    QString sessionDirName("session");

    if (!parentDir.mkpath(sessionDirName)) {
        SVCERR << "ERROR: makeSessionFilename: Failed to create session dir in \"" << parentDir.canonicalPath() << "\"" << endl;
        QMessageBox::critical(this, tr("Failed to create session directory"),
                              tr("<p>Failed to create directory \"%1\" for session files</p>")
                              .arg(parentDir.filePath(sessionDirName)));
        return {};
    }

    QDir sessionDir(parentDir.filePath(sessionDirName));
    
    QDateTime now = QDateTime::currentDateTime();
    QString dateDirName = QString("%1").arg(now.toString("yyyyMMdd"));

    if (!sessionDir.mkpath(dateDirName)) {
        SVCERR << "ERROR: makeSessionFilename: Failed to create datestamped session dir in \"" << sessionDir.canonicalPath() << "\"" << endl;
        QMessageBox::critical(this, tr("Failed to create session directory"),
                              tr("<p>Failed to create date directory \"%1\" for session files</p>")
                              .arg(sessionDir.filePath(dateDirName)));
        return {};
    }

    QDir dateDir(sessionDir.filePath(dateDirName));

    QString sessionName = mainModel->getTitle();
    if (sessionName == "") {
        sessionName = mainModel->getLocation();
    }
    sessionName = QFileInfo(sessionName).baseName();

    QString sessionExt = 
        InteractiveFileFinder::getInstance()->getApplicationSessionExtension();

    QString filePath = dateDir.filePath(QString("%1.%2")
                                        .arg(sessionName)
                                        .arg(sessionExt));
    int suffix = 0;
    while (QFile(filePath).exists()) {
        if (++suffix == 100) {
            SVCERR << "ERROR: makeSessionFilename: Failed to come up with unique session filename for " << sessionName << endl;
            return {};
        }
        filePath = dateDir.filePath(QString("%1-%2.%3")
                                    .arg(sessionName)
                                    .arg(suffix)
                                    .arg(sessionExt));
    }

    SVDEBUG << "MainWindow::makeSessionFilename: returning "
            << filePath << endl;

    return filePath;
}

QString
MainWindow::makeSessionLabel()
{
    Model *mainModel = getMainModel();
    if (!mainModel) {
        SVDEBUG << "MainWindow::makeSessionFilename: No main model, returning empty filename" << endl;
        return {};
    }

    QString sessionName = mainModel->getTitle();
    if (sessionName == "") {
        sessionName = mainModel->getLocation();
        sessionName = QFileInfo(sessionName).baseName();
    }

    int paneCount = 1;
    if (m_paneStack) paneCount = m_paneStack->getPaneCount();
    QString label = tr("%1: %n file(s)", "", paneCount).arg(sessionName);
    
    SVDEBUG << "MainWindow::makeSessionLabel: returning "
            << label << endl;

    return label;
}

void
MainWindow::checkpointSession()
{
    if (m_sessionState == NoSession) {
        SVCERR << "MainWindow::checkpointSession: no current session" << endl;
        return;
    }

    if (m_sessionState == SessionLoading) {
        SVCERR << "MainWindow::checkpointSession: session is loading" << endl;
        return;
    }
    
    // This test is necessary, so that we don't get into a nasty loop
    // when checkpointing on closeSession called when opening a new
    // session file
    if (!m_documentModified) {
        SVCERR << "MainWindow::checkpointSession: nothing to save" << endl;
        return;
    }
    
    if (m_sessionFile == "") {
        SVCERR << "MainWindow::checkpointSession: no current session file" << endl;
        return;
    }

    QString sessionExt = 
        InteractiveFileFinder::getInstance()->getApplicationSessionExtension();

    if (!m_sessionFile.endsWith("." + sessionExt)) {
        // At one point in development we had a nasty situation where
        // we loaded an audio file from the recent files list, then
        // immediately saved the session over the top of it! This is
        // just an additional guard against that kind of thing
        SVCERR << "MainWindow::checkpointSession: suspicious session filename "
               << m_sessionFile << ", not saving to it" << endl;
        return;

        //!!! + we should also check that it is actually in our
        //!!! auto-save session directory
    }
    
    SVCERR << "MainWindow::checkpointSession: saving to session file: "
           << m_sessionFile << endl;

    SmallSession session(makeSmallSession());

    try {
        SmallSession::save(session, m_sessionFile);
        m_recentSessions.addFile(m_sessionFile, makeSessionLabel());
        CommandHistory::getInstance()->documentSaved();
        documentRestored();
        SVCERR << "MainWindow::checkpointSession complete" << endl;
    } catch (const std::runtime_error &e) {
        SVCERR << "MainWindow::checkpointSession: save failed: "
               << e.what() << endl;
    }
}

SmallSession
MainWindow::makeSmallSession()
{
    SmallSession session;
    if (!m_paneStack) return session;

    WaveFileModel *mainModel = getMainModel();
    if (!mainModel) return session;

    session.mainFile = mainModel->getLocation();

    std::set<QString> alreadyRecorded;
    alreadyRecorded.insert(session.mainFile);
    
    for (int i = 0; i < m_paneStack->getPaneCount(); ++i) {
        Pane *p = m_paneStack->getPane(i);
        for (int j = 0; j < p->getLayerCount(); ++j) {
            Layer *l = p->getLayer(j);
            Model *m = l->getModel();
            while (m && m->getSourceModel()) m = m->getSourceModel();
            if (qobject_cast<WaveFileModel *>(m)) {
                QString location = m->getLocation();
                if (alreadyRecorded.find(location) == alreadyRecorded.end()) {
                    session.additionalFiles.push_back(location);
                    alreadyRecorded.insert(location);
                }
            }
        }
    }

    SVCERR << "MainWindow::makeSmallSession: have " << session.additionalFiles.size() << " non-main model(s)" << endl;
    
    return session;
}

void
MainWindow::mainModelChanged(WaveFileModel *model)
{
    SVDEBUG << "MainWindow::mainModelChanged(" << model << ")" << endl;

    if (m_sessionState == SessionLoading) {
        SVDEBUG << "MainWindow::mainModelChanged: Session is loading, not (re)making session filename" << endl;
    } else if (!model) {
        SVDEBUG << "MainWindow::mainModelChanged: Null model, not (re)making session filename" << endl;
    } else {
        if (m_sessionState == NoSession) {
            SVDEBUG << "MainWindow::mainModelChanged: Marking session as active" << endl;
            m_sessionState = SessionActive;
        } else {
            SVDEBUG << "MainWindow::mainModelChanged: Session is active" << endl;
        }
        if (m_sessionFile == "") {
            SVDEBUG << "MainWindow::mainModelChanged: No session file set, calling makeSessionFilename" << endl;
            m_sessionFile = makeSessionFilename();
        }
    }
    
    m_salientPending.clear();
    m_salientCalculating = false;

    MainWindowBase::mainModelChanged(model);

    if (m_playTarget || m_audioIO) {
        connect(m_mainLevelPan, SIGNAL(levelChanged(float)),
                this, SLOT(mainModelGainChanged(float)));
        connect(m_mainLevelPan, SIGNAL(panChanged(float)),
                this, SLOT(mainModelPanChanged(float)));
    }

    SVDEBUG << "Pane stack pane count = " << m_paneStack->getPaneCount() << endl;

    if (model &&
        m_paneStack &&
        (m_paneStack->getPaneCount() == 0)) {
        
        AddPaneCommand *command = new AddPaneCommand(this);
        CommandHistory::getInstance()->addCommand(command);
        Pane *pane = command->getPane();
        Layer *newLayer =
            m_document->createMainModelLayer(LayerFactory::Waveform);
        newLayer->setObjectName(tr("Outline Waveform"));
        
        bool mono = (model->getChannelCount() == 1);
        
        QString layerPropertyXml =
            QString("<layer scale=\"%1\" channelMode=\"%2\"/>")
            .arg(int(WaveformLayer::MeterScale))
            .arg(int(mono ?
                     WaveformLayer::SeparateChannels :
                     WaveformLayer::MergeChannels));
        LayerFactory::getInstance()->setLayerProperties
            (newLayer, layerPropertyXml);
            
        m_document->addLayerToView(pane, newLayer);

        addSalientFeatureLayer(pane, model);
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
    checkpointSession();
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
        "<p>Sonic Vector Copyright &copy; 2005 - 2019 Chris Cannam and<br>"
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

#ifdef Q_OS_MAC    
    QString stylepath = ":vect-mac.qss";
#else
    QString stylepath = ":vect.qss";
#endif

    QFile file(stylepath);
    if (!file.open(QFile::ReadOnly)) {
        SVCERR << "WARNING: Failed to open style file " << stylepath << endl;
    } else {
        QString styleSheet = QLatin1String(file.readAll());
        qApp->setStyleSheet(styleSheet);
        QPalette pal(Qt::white, Qt::gray, Qt::white, Qt::black, Qt::gray, Qt::white, Qt::white, Qt::black, Qt::black);
        qApp->setPalette(pal);
    }
}



