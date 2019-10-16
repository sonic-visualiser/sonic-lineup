/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Lineup
    Comparative visualisation and alignment of related audio recordings
    Centre for Digital Music, Queen Mary, University of London.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "../version.h"

#include "MainWindow.h"
#include "framework/Document.h"
#include "framework/VersionTester.h"

#include "PreferencesDialog.h"
#include "NetworkPermissionTester.h"
#include "IntroDialog.h"

#include "view/Pane.h"
#include "view/PaneStack.h"
#include "data/model/WaveFileModel.h"
#include "data/model/SparseOneDimensionalModel.h"
#include "data/model/AlignmentModel.h"
#include "data/model/SparseOneDimensionalModel.h"
#include "base/StorageAdviser.h"
#include "base/Exceptions.h"
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
#include "layer/SpectrogramLayer.h"
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
#include <QDialogButtonBox>
#include <QTextEdit>

#include <iostream>
#include <cstdio>
#include <errno.h>

using std::cerr;
using std::endl;

using std::vector;
using std::map;
using std::set;
using std::pair;


MainWindow::MainWindow(AudioMode audioMode) :
    MainWindowBase(audioMode,
                   MainWindowBase::MIDI_NONE,
                   int(PaneStack::Option::NoUserResize) |
                   int(PaneStack::Option::NoPropertyStacks) |
                   int(PaneStack::Option::ShowAlignmentViews) |
                   int(PaneStack::Option::NoCloseOnFirstPane)),
    m_mainMenusCreated(false),
    m_playbackMenu(nullptr),
    m_recentSessionsMenu(nullptr),
    m_deleteSelectedAction(nullptr),
    m_ffwdAction(nullptr),
    m_rwdAction(nullptr),
    m_recentSessions("RecentSessions", 20),
    m_exiting(false),
    m_preferencesDialog(nullptr),
    m_layerTreeView(nullptr),
    m_keyReference(new KeyReference()),
    m_versionTester(nullptr),
    m_networkPermission(false),
    m_displayMode(OutlineWaveformMode),
    m_salientCalculating(false),
    m_salientColour(0),
    m_sessionState(NoSession)
{
    setWindowTitle(QApplication::applicationName());

    setUnifiedTitleAndToolBarOnMac(true);

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

    settings.beginGroup("Alignment");
    if (!settings.contains("align-pitch-aware")) {
        settings.setValue("align-pitch-aware", true);
    }
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
    m_modeDisplayOrder.push_back(OutlineWaveformMode);

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
    m_modeDisplayOrder.push_back(WaveformMode);

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
    m_modeDisplayOrder.push_back(MelodogramMode);

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
    m_modeDisplayOrder.push_back(SpectrogramMode);

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
    m_modeDisplayOrder.push_back(PitchMode);

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
    m_modeDisplayOrder.push_back(KeyMode);

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
    m_modeDisplayOrder.push_back(AzimuthMode);

    m_playSpeed = new AudioDial(bottomFrame);
    m_playSpeed->setMinimum(0);
    m_playSpeed->setMaximum(120);
    m_playSpeed->setValue(60);
    m_playSpeed->setFixedWidth(int(bottomElementHeight * 0.9));
    m_playSpeed->setFixedHeight(int(bottomElementHeight * 0.9));
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

    NetworkPermissionTester tester;
    m_networkPermission = tester.havePermission();

    if (!reopenLastSession()) {
        QTimer::singleShot(400, this, SLOT(introDialog()));
    } else {
        // Do this here only if not showing the intro dialog -
        // otherwise the introDialog function will do this after it
        // has shown the dialog, so we don't end up with both at once
        checkForNewerVersion();
    }
                       
    QTimer::singleShot(500, this, SLOT(betaReleaseWarning()));
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
        m_scrollLeftAction, m_scrollRightAction,
        m_selectPreviousPaneAction, m_selectNextPaneAction,
        m_selectPreviousDisplayModeAction, m_selectNextDisplayModeAction
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
    
    action = new QAction(tr("Browse Recorded Audio"), this);
    action->setStatusTip(tr("Open the Recorded Audio folder in the system file browser"));
    connect(action, SIGNAL(triggered()), this, SLOT(browseRecordedAudio()));
    menu->addAction(action);

    menu->addSeparator();

    m_recentSessionsMenu = menu->addMenu(tr("&Recent Sessions"));
    m_recentSessionsMenu->setTearOffEnabled(false);
    setupRecentSessionsMenu();
    connect(&m_recentSessions, SIGNAL(recentChanged()),
            this, SLOT(setupRecentSessionsMenu()));
    
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
    action->setStatusTip(tr("Exit Sonic Lineup"));
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

    action = new QAction(tr("Switch to Previous Pane"), this);
    action->setShortcut(tr("["));
    action->setStatusTip(tr("Make the next pane up in the pane stack current"));
    connect(action, SIGNAL(triggered()), this, SLOT(previousPane()));
    connect(this, SIGNAL(canSelectPreviousPane(bool)), action, SLOT(setEnabled(bool)));
    m_keyReference->registerShortcut(action);
    menu->addAction(action);
    m_selectPreviousPaneAction = action;

    action = new QAction(tr("Switch to Next Pane"), this);
    action->setShortcut(tr("]"));
    action->setStatusTip(tr("Make the next pane down in the pane stack current"));
    connect(action, SIGNAL(triggered()), this, SLOT(nextPane()));
    connect(this, SIGNAL(canSelectNextPane(bool)), action, SLOT(setEnabled(bool)));
    m_keyReference->registerShortcut(action);
    menu->addAction(action);
    m_selectNextPaneAction = action;

    menu->addSeparator();

    action = new QAction(tr("Switch to Previous View Mode"), this);
    action->setShortcut(tr("{"));
    action->setStatusTip(tr("Make the next view mode current"));
    connect(action, SIGNAL(triggered()), this, SLOT(previousDisplayMode()));
    connect(this, SIGNAL(canSelectPreviousDisplayMode(bool)), action, SLOT(setEnabled(bool)));
    m_keyReference->registerShortcut(action);
    menu->addAction(action);
    m_selectPreviousDisplayModeAction = action;

    action = new QAction(tr("Switch to Next View Mode"), this);
    action->setShortcut(tr("}"));
    action->setStatusTip(tr("Make the next view mode current"));
    connect(action, SIGNAL(triggered()), this, SLOT(nextDisplayMode()));
    connect(this, SIGNAL(canSelectNextDisplayMode(bool)), action, SLOT(setEnabled(bool)));
    m_keyReference->registerShortcut(action);
    menu->addAction(action);
    m_selectNextDisplayModeAction = action;

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
    m_viewManager->setOverlayMode(ViewManager::NoOverlays);
    m_keyReference->registerShortcut(action);
    menu->addAction(action);

    // We need this separator even if not adding the full-screen
    // option ourselves, as the Mac automatic full-screen entry
    // doesn't include a separator first
    menu->addSeparator();

#ifndef Q_OS_MAC
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

    QString name = QApplication::applicationName();
    
    action = new QAction(tr("What's &New In This Release?"), this); 
    action->setStatusTip(tr("List the changes in this release (and every previous release) of %1").arg(name)); 
    connect(action, SIGNAL(triggered()), this, SLOT(whatsNew()));
    menu->addAction(action);
    
    action = new QAction(tr("&About %1").arg(name), this); 
    action->setStatusTip(tr("Show information about %1").arg(name)); 
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
    tdAction->setChecked(settings.value("align-pitch-aware", false).toBool());
    settings.endGroup();
    tdAction->setStatusTip(tr("Compare relative pitch content of audio files before aligning, in order to correctly align recordings of the same material at different tuning pitches"));
    connect(tdAction, SIGNAL(triggered()), this, SLOT(tuningDifferenceToggled()));

    menu->addSeparator();
    menu->addAction(alAction);
    menu->addAction(tdAction);

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

    emit canSelectPreviousDisplayMode
        (!m_modeDisplayOrder.empty() &&
         (m_displayMode != m_modeDisplayOrder[0]));

    emit canSelectNextDisplayMode
        (!m_modeDisplayOrder.empty() &&
         (m_displayMode != m_modeDisplayOrder[m_modeDisplayOrder.size()-1]));

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
    zoomDefault();

    // Record that the last (i.e. current, as of now) session is
    // empty, so that if we exit now and re-start, we get an empty
    // session as is proper instead of loading the last non-empty one
    QSettings settings;
    settings.beginGroup("MainWindow");
    settings.setValue("lastsession", "");
    settings.endGroup();
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

    setWindowTitle(tr("Sonic Lineup"));

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

    if (paths.empty()) return;
    
    m_sessionState = SessionActive;
    
    for (QString path: paths) {

        FileOpenStatus status = FileOpenFailed;
        
        FileSource source(path);
        if (source.isAvailable()) {
            source.waitForData();

            try {
                status = openAudio(source, CreateAdditionalModel);
            } catch (const InsufficientDiscSpace &e) {
                SVCERR << "MainWindowBase: Caught InsufficientDiscSpace in file open" << endl;
                QMessageBox::critical
                    (this, tr("Not enough disc space"),
                     tr("<b>Not enough disc space</b><p>There doesn't appear to be enough spare disc space to accommodate any necessary temporary files.</p><p>Please clear some space and try again.</p>").arg(e.what()));
                return;
            }
        }
        
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
    if (text.isEmpty()) return;

    m_sessionState = SessionActive;

    FileOpenStatus status = FileOpenFailed;
        
    FileSource source(text);
    if (source.isAvailable()) {
        source.waitForData();

        try {
            status = openAudio(source, CreateAdditionalModel);
        } catch (const InsufficientDiscSpace &e) {
            SVCERR << "MainWindowBase: Caught InsufficientDiscSpace in file open" << endl;
            QMessageBox::critical
                (this, tr("Not enough disc space"),
                 tr("<b>Not enough disc space</b><p>There doesn't appear to be enough spare disc space to accommodate any necessary temporary files.</p><p>Please clear some space and try again.</p>").arg(e.what()));
            return;
        }
    }
        
    if (status != FileOpenSucceeded) {
        QMessageBox::critical(this, tr("Failed to open location"),
                              tr("<b>Open failed</b><p>URL \"%1\" could not be opened").arg(text));
    } else {
        configureNewPane(m_paneStack->getCurrentPane());
        settings.setValue("lastremote", text);
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

bool
MainWindow::reopenLastSession()
{
    QSettings settings;
    settings.beginGroup("MainWindow");
    QString lastSession = settings.value("lastsession", "").toString();
    settings.endGroup();

    if (lastSession != "") {
        openSmallSessionFile(lastSession);
    }

    if (m_sessionState == NoSession) {
        newSession(); // to ensure we have a drop target
        return false;
    } else {
        return true;
    }
}

void
MainWindow::openSmallSessionFile(QString path)
{
    m_sessionFile = path;
    m_sessionState = SessionLoading;

    SVDEBUG << "MainWindow::openSmallSessionFile: m_sessionFile is now "
            << m_sessionFile << endl;

    try {
        SmallSession session = SmallSession::load(path);
        openSmallSession(session);
    } catch (const std::runtime_error &e) {
        QMessageBox::critical
            (this, tr("Failed to reload session"),
             tr("<b>Open failed</b>"
                "<p>Session file \"%1\" could not be opened: %2</p>")
             .arg(path).arg(e.what()));
        m_sessionFile = "";
        m_sessionState = NoSession;
    }
}

void
MainWindow::openSmallSession(const SmallSession &session)
{
    QString errorText;
    FileOpenStatus status;
    
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

        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        
        status = openPath(path, CreateAdditionalModel);

        if (status != FileOpenSucceeded) {
            errorText = tr("Unable to open audio file %1").arg(path);
            goto failed;
        }

        configureNewPane(m_paneStack->getCurrentPane());
    }

    rewindStart();
    
    m_documentModified = false;
    m_sessionState = SessionActive;
    return;

failed:
    QMessageBox::critical(this, tr("Failed to load session"),
                          tr("<b>Open failed</b><p>Session could not be opened: %2</p>").arg(errorText));
    m_sessionFile = "";
    m_sessionState = NoSession;
}

bool
MainWindow::selectExistingLayerForMode(Pane *pane,
                                       QString modeName,
                                       ModelId *createFrom)
{
    // Search the given pane for any layer whose object name matches
    // modeName, showing it if it exists, and hiding all other layers
    // (except for time-instants layers, which are assumed to be used
    // for persistent segment display and are left unmodified).

    // In the case where no such layer is found and false is returned,
    // then if the return parameter createFrom is non-null, the value
    // it points to will be set to a pointer to the model from which
    // such a layer should be constructed.

    ModelId modelId;

    bool have = false;

    for (int i = 0; i < pane->getLayerCount(); ++i) {
        
        Layer *layer = pane->getLayer(i);
        if (!layer || qobject_cast<TimeInstantLayer *>(layer)) {
            continue;
        }

        modelId = layer->getModel();
        auto sourceId = layer->getSourceModel();
        if (!sourceId.isNone()) modelId = sourceId;
        
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
        *createFrom = modelId;
    }
    return false;
}

void
MainWindow::addSalientFeatureLayer(Pane *pane, ModelId modelId)
{
    //!!! what if there already is one? could have changed the main
    //!!! model for example

    auto model = ModelById::getAs<WaveFileModel>(modelId);
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

    m_salientCalculating = true;

    Transform transform = tf->getDefaultTransformFor
        (id, model->getSampleRate());

    ModelTransformer::Input input(modelId, -1);

    Layer *newLayer = m_document->createDerivedLayer(transform, modelId);

    if (newLayer) {

        TimeInstantLayer *til = qobject_cast<TimeInstantLayer *>(newLayer);
        if (til) {
            til->setPlotStyle(TimeInstantLayer::PlotInstants);
            til->setBaseColour(m_salientColour);
        }

        auto params = newLayer->getPlayParameters();
        if (params) {
            params->setPlayAudible(false);
        }

        connect(til, SIGNAL(modelCompletionChanged(ModelId)),
                this, SLOT(salientLayerCompletionChanged(ModelId)));
        
        m_document->addLayerToView(pane, newLayer);
        m_paneStack->setCurrentLayer(pane, newLayer);
    }
}

void
MainWindow::salientLayerCompletionChanged(ModelId)
{
    Layer *layer = qobject_cast<Layer *>(sender());
    if (layer && layer->getCompletion(0) == 100) {
        m_salientCalculating = false;
        for (ModelId am: m_salientPending) {
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
                if (l->getModel() == getMainModelId()) {
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
MainWindow::mapSalientFeatureLayer(ModelId amId)
{
    auto am = ModelById::getAs<AlignmentModel>(amId);
    if (!am) {
        SVCERR << "MainWindow::mapSalientFeatureLayer: AlignmentModel is absent!"
               << endl;
        return;
    }
    
    if (m_salientCalculating) {
        m_salientPending.insert(amId);
        return;
    }

    TimeInstantLayer *salient = findSalientFeatureLayer();
    if (!salient) {
        SVCERR << "MainWindow::mapSalientFeatureLayer: No salient layer found"
               << endl;
        m_salientPending.insert(amId);
        return;
    }
    
    ModelId modelId = am->getAlignedModel();
    auto model = ModelById::get(modelId);
    if (!model) {
        SVCERR << "MainWindow::mapSalientFeatureLayer: No aligned model in AlignmentModel" << endl;
        return;
    }

    Pane *pane = nullptr;
    Layer *layer = nullptr;
    Pane *firstPane = nullptr;
    
    for (int i = 0; i < m_paneStack->getPaneCount(); ++i) {
        Pane *p = m_paneStack->getPane(i);
        if (p && !firstPane) firstPane = p;
        for (int j = 0; j < p->getLayerCount(); ++j) {
            Layer *l = p->getLayer(j);
            if (!l) continue;
            if (l->getModel() == modelId) {
                pane = p;
                layer = l;
                break;
            }
        }
        if (layer) break;
    }

    if (!pane || !layer) {
        SVCERR << "MainWindow::mapSalientFeatureLayer: Failed to find model "
               << modelId << " in any layer" << endl;
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

    pane->setCentreFrame(model->alignFromReference(firstPane->getCentreFrame()));

    auto fromId = salient->getModel();
    auto from = ModelById::getAs<SparseOneDimensionalModel>(fromId);
    if (!from) {
        SVCERR << "MainWindow::mapSalientFeatureLayer: "
               << "Salient layer lacks SparseOneDimensionalModel" << endl;
        return;
    }

    auto to = std::make_shared<SparseOneDimensionalModel>
        (model->getSampleRate(), from->getResolution(), false);
    auto toId = ModelById::add(to);

    EventVector pp = from->getAllEvents();
    for (const auto &p: pp) {
        Event aligned = p
            .withFrame(model->alignFromReference(p.getFrame()))
            .withLabel(""); // remove label, as the analysis was not
                            // conducted on the audio we're mapping to
        to->add(aligned);
    }

    Layer *newLayer = m_document->createImportedLayer(toId);

    if (newLayer) {

        newLayer->setObjectName(salientLayerName);
        
        TimeInstantLayer *til = qobject_cast<TimeInstantLayer *>(newLayer);
        if (til) {
            til->setPlotStyle(TimeInstantLayer::PlotInstants);
            til->setBaseColour(m_salientColour);
        }
        
        auto params = newLayer->getPlayParameters();
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

        ModelId createFrom;
        if (!selectExistingLayerForMode(pane, name, &createFrom) &&
            !createFrom.isNone()) {

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

        ModelId createFrom;
        if (!selectExistingLayerForMode(pane, name, &createFrom) &&
            !createFrom.isNone()) {

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

        ModelId createFrom;
        if (!selectExistingLayerForMode(pane, name, &createFrom) &&
            !createFrom.isNone()) {
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

        ModelId createFrom;
        if (!selectExistingLayerForMode(pane, name, &createFrom) &&
            !createFrom.isNone()) {
            Layer *newLayer = m_document->createLayer
                (LayerFactory::MelodicRangeSpectrogram);
            SpectrogramLayer *spectrogram = qobject_cast<SpectrogramLayer *>
                (newLayer);
            spectrogram->setVerticallyFixed();
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
                                      Transform::ParameterMap parameters,
                                      QString layerPropertyXml,
                                      bool includeGhostReference)
{
    QString name = m_modeLayerNames[mode];

    // Bring forth any existing layers of the appropriate name; for
    // each pane that lacks one, make a note of the model from which
    // we should create it

    map<Pane *, ModelId> sourceModels;
    
    for (int i = 0; i < m_paneStack->getPaneCount(); ++i) {
        Pane *pane = m_paneStack->getPane(i);
        ModelId createFrom;
        if (!selectExistingLayerForMode(pane, name, &createFrom)) {
            if (!createFrom.isNone()) {
                sourceModels[pane] = createFrom;
            }
        }
    }

    Layer *ghostReference = nullptr;

    if (includeGhostReference && !sourceModels.empty()) {

        // Look up the layer of this type in the first pane -- this is
        // the reference that we must include as a ghost in the pane
        // that we're adding the new layer to.

        // NB it won't exist if this is the first time into this mode
        // and we haven't created the layer for the reference pane yet
        // - we have to handle that in the creation loop below.
        
        Pane *pane = m_paneStack->getPane(0);
        
        for (int i = 0; i < pane->getLayerCount(); ++i) {
            Layer *layer = pane->getLayer(i);
            if (!layer || qobject_cast<TimeInstantLayer *>(layer)) {
                continue;
            }
            if (layer->objectName() == name) {
                ghostReference = layer;
                break;
            }
        }
    }

    Pane *currentPane = m_paneStack->getCurrentPane();

    TransformFactory *tf = TransformFactory::getInstance();

    if (tf->haveTransform(transformId)) {

        for (int i = 0; i < m_paneStack->getPaneCount(); ++i) {

            Pane *pane = m_paneStack->getPane(i);

            if (sourceModels.find(pane) == sourceModels.end()) {
                // no need to create, this one exists already
                continue;
            }

            ModelId source = sourceModels[pane];

            if (ghostReference) {
                m_document->addLayerToView(pane, ghostReference);
                pane->setUseAligningProxy(true);
            }
            
            Transform transform = tf->getDefaultTransformFor(transformId);

            if (!parameters.empty()) {
                transform.setParameters(parameters);
            }

            ModelTransformer::Input input(source, -1);

            Layer *layer = m_document->createDerivedLayer(transform, source);

            if (layer) {

                layer->setObjectName(name);
                LayerFactory::getInstance()->setLayerProperties
                    (layer, layerPropertyXml);

                SingleColourLayer *scl =
                    qobject_cast<SingleColourLayer *>(layer);
                if (scl) {
                    int colourIndex = 
                        (i % ColourDatabase::getInstance()->getColourCount());
                    scl->setBaseColour(colourIndex);
                }

                m_document->addLayerToView(pane, layer);
                m_paneStack->setCurrentLayer(pane, layer);

                if (!ghostReference && includeGhostReference && i == 0) {
                    ghostReference = layer;
                }
                
            } else {
                SVCERR << "ERROR: Failed to create derived layer" << endl;
            }

            TimeInstantLayer *salient = findSalientFeatureLayer(pane);
            if (salient) {
                pane->propertyContainerSelected(pane, salient);
            }
        }
    } else {
        SVCERR << "ERROR: No plugin available for mode: " << name << endl;
    }

    if (currentPane) {
        m_paneStack->setCurrentPane(currentPane);
    }

    m_displayMode = mode;
    checkpointSession();
}

void
MainWindow::pitchModeSelected()
{
    QString propertyXml =
        QString("<layer plotStyle=\"%1\" verticalScale=\"%2\" scaleMinimum=\"%3\" scaleMaximum=\"%4\"/>")
        .arg(int(TimeValueLayer::PlotDiscreteCurves))
        .arg(int(TimeValueLayer::LogScale))
        .arg(40)
        .arg(510);
    
    selectTransformDrivenMode
        (PitchMode,
         "vamp:pyin:pyin:smoothedpitchtrack",
         {},
         propertyXml,
         true); // ghost reference
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
         "vamp:qm-vamp-plugins:qm-keydetector:mergedkeystrength",
         {},
         propertyXml,
         false);
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
         {},
         propertyXml,
         false);
}

void
MainWindow::previousDisplayMode()
{
    for (int i = 0; in_range_for(m_modeDisplayOrder, i); ++i) {
        if (m_displayMode == m_modeDisplayOrder[i]) {
            if (i > 0) {
                m_modeButtons[m_modeDisplayOrder[i-1]]->click();
            }
            break;
        }
    }
}

void
MainWindow::nextDisplayMode()
{
    for (int i = 0; in_range_for(m_modeDisplayOrder, i); ++i) {
        if (m_displayMode == m_modeDisplayOrder[i]) {
            if (in_range_for(m_modeDisplayOrder, i+1)) {
                m_modeButtons[m_modeDisplayOrder[i+1]]->click();
            }
            break;
        }
    }
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

    m_sessionState = SessionActive;
    
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
    
    for (QString uri: uriList) {

        FileOpenStatus status = FileOpenFailed;
        
        FileSource source(uri);
        if (source.isAvailable()) {
            source.waitForData();

            try {
                status = openAudio(source, CreateAdditionalModel);
            } catch (const InsufficientDiscSpace &e) {
                SVCERR << "MainWindowBase: Caught InsufficientDiscSpace in file open" << endl;
                QMessageBox::critical
                    (this, tr("Not enough disc space"),
                     tr("<b>Not enough disc space</b><p>There doesn't appear to be enough spare disc space to accommodate any necessary temporary files.</p><p>Please clear some space and try again.</p>").arg(e.what()));
                return;
            }
        }
            
        if (status != FileOpenSucceeded) {
            QMessageBox::critical(this, tr("Failed to open dropped URL"),
                                  tr("<b>Open failed</b><p>Dropped audio file location \"%1\" could not be opened").arg(uri));
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

    // MainWindowBase::addOpenedAudioModel adds a waveform layer for
    // each additional model besides the main one (assuming that the
    // main one gets it, if needed, from the session template). We
    // don't actually want to use those - we'll be adding our own with
    // specific parameters - but they don't cost much, so rather than
    // remove them, just rename them to something that won't cause
    // confusion with the name-based mode layer handling. NB we have
    // to do this before calling reselectMode(), as that will add
    // another competing waveform layer
    
    Layer *waveformLayer = 0;

    for (int i = 0; i < pane->getLayerCount(); ++i) {
        Layer *layer = pane->getLayer(i);
        if (!layer) {
            continue;
        }
        if (dynamic_cast<WaveformLayer *>(layer)) {
            waveformLayer = layer;
        }
        if (dynamic_cast<TimeValueLayer *>(layer)) {
            break;
        }
    }

    if (waveformLayer) {
        waveformLayer->setObjectName("Automatically Created - Unused"); // not to be translated
    }

    zoomToFit();
    reselectMode();
}

void
MainWindow::record()
{
    MainWindowBase::record();
    configureNewPane(m_paneStack->getCurrentPane());
    zoomDefault();
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
MainWindow::introDialog()
{
    IntroDialog::show(this);
    
    // and now that's out of the way
    checkForNewerVersion();
}

void
MainWindow::checkForNewerVersion()
{
    if (m_networkPermission) {
        SVDEBUG << "Network permission granted: checking for updates" << endl;
        m_versionTester = new VersionTester
            ("sonicvisualiser.org", "latest-vect-version.txt", VECT_VERSION);
        connect(m_versionTester, SIGNAL(newerVersionAvailable(QString)),
                this, SLOT(newerVersionAvailable(QString)));
    } else {
        SVDEBUG << "Network permission not granted: not checking for updates"
                << endl;
    }
}

void
MainWindow::betaReleaseWarning()
{
    QMessageBox::information
        (this, tr("Beta release"),
         tr("<b>This is a beta release of %1</b><p>Please see the \"What's New\" option in the Help menu for a list of changes since the last proper release.</p>").arg(QApplication::applicationName()));
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
MainWindow::modelAdded(ModelId model)
{
    MainWindowBase::modelAdded(model);
}

QString
MainWindow::makeSessionFilename()
{
    auto mainModel = getMainModel();
    if (!mainModel) {
        SVDEBUG << "MainWindow::makeSessionFilename: No main model, returning empty filename" << endl;
        return {};
    }
    
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
    sessionName.replace(QRegExp("[<>:\"/\\\\|?*\\0000-\\0039()\\[\\]$]"), "_");

    QString sessionExt = 
        InteractiveFileFinder::getInstance()->getApplicationSessionExtension();

    QString filePath = dateDir.filePath(QString("%1.%2")
                                        .arg(sessionName)
                                        .arg(sessionExt));
    int suffix = 0;
    while (QFile(filePath).exists()) {
        if (++suffix == 100) {
            SVCERR << "ERROR: makeSessionFilename: Failed to come up with unique session filename for " << sessionName << endl;
            QMessageBox::critical(this, tr("Failed to obtain unique filename"),
                                  tr("<p>Failed to obtain a unique filename for session file</p>"));
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
    auto mainModel = getMainModel();
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

        QSettings settings;
        settings.beginGroup("MainWindow");
        settings.setValue("lastsession", m_sessionFile);
        settings.endGroup();

        SVCERR << "MainWindow::checkpointSession complete" << endl;
        
    } catch (const std::runtime_error &e) {
        SVCERR << "MainWindow::checkpointSession: save failed: "
               << e.what() << endl;
        QMessageBox::critical
            (this, tr("Failed to checkpoint session"),
             tr("<b>Checkpoint failed</b>"
                "<p>Session checkpoint file could not be saved: %1</p>")
             .arg(e.what()));
    }
}

SmallSession
MainWindow::makeSmallSession()
{
    SmallSession session;
    if (!m_paneStack) return session;

    auto mainModel = getMainModel();
    if (!mainModel) return session;

    session.mainFile = mainModel->getLocation();

    std::set<QString> alreadyRecorded;
    alreadyRecorded.insert(session.mainFile);
    
    for (int i = 0; i < m_paneStack->getPaneCount(); ++i) {
        Pane *p = m_paneStack->getPane(i);
        for (int j = 0; j < p->getLayerCount(); ++j) {
            Layer *l = p->getLayer(j);
            auto modelId = l->getModel();
            auto sourceId = l->getSourceModel();
            if (!sourceId.isNone()) {
                modelId = sourceId;
            }
            if (auto wfm = ModelById::getAs<WaveFileModel>(modelId)) {
                QString location = wfm->getLocation();
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
MainWindow::mainModelChanged(ModelId modelId)
{
    SVDEBUG << "MainWindow::mainModelChanged(" << modelId << ")" << endl;

    if (m_sessionState == SessionLoading) {
        SVDEBUG << "MainWindow::mainModelChanged: Session is loading, not (re)making session filename" << endl;
    } else if (modelId.isNone()) {
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

    MainWindowBase::mainModelChanged(modelId);

    if (m_playTarget || m_audioIO) {
        connect(m_mainLevelPan, SIGNAL(levelChanged(float)),
                this, SLOT(mainModelGainChanged(float)));
        connect(m_mainLevelPan, SIGNAL(panChanged(float)),
                this, SLOT(mainModelPanChanged(float)));
    }

    SVDEBUG << "Pane stack pane count = " << m_paneStack->getPaneCount() << endl;

    auto model = ModelById::getAs<WaveFileModel>(modelId);
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

        addSalientFeatureLayer(pane, modelId);
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
MainWindow::alignmentComplete(ModelId modelId)
{
    cerr << "MainWindow::alignmentComplete(" << modelId << ")" << endl;
    mapSalientFeatureLayer(modelId);
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
    openHelpUrl(tr("http://www.sonicvisualiser.org/sonic-lineup/"));
}

void
MainWindow::help()
{
    openHelpUrl(tr("http://www.sonicvisualiser.org/sonic-lineup/doc/reference/%1/en/").arg(VECT_VERSION));
}

void
MainWindow::whatsNew()
{
    QFile changelog(":CHANGELOG");
    changelog.open(QFile::ReadOnly);
    QByteArray content = changelog.readAll();
    QString text = QString::fromUtf8(content);

    QDialog *d = new QDialog(this);
    d->setWindowTitle(tr("What's New"));
        
    QGridLayout *layout = new QGridLayout;
    d->setLayout(layout);

    int row = 0;
    
    QLabel *iconLabel = new QLabel;
    iconLabel->setPixmap(QApplication::windowIcon().pixmap(64, 64));
    layout->addWidget(iconLabel, row, 0);
    
    layout->addWidget
        (new QLabel(tr("<h3>What's New in %1</h3>")
                    .arg(QApplication::applicationName())),
         row++, 1);
    layout->setColumnStretch(2, 10);

    QTextEdit *textEdit = new QTextEdit;
    layout->addWidget(textEdit, row++, 1, 1, 2);

    if (m_newerVersionIs != "") {
        layout->addWidget(new QLabel(tr("<b>Note:</b> A newer version of %1 is available.<br>(Version %2 is available; you are using version %3)").arg(QApplication::applicationName()).arg(m_newerVersionIs).arg(VECT_VERSION)), row++, 1, 1, 2);
    }
    
    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok);
    layout->addWidget(bb, row++, 0, 1, 3);
    connect(bb, SIGNAL(accepted()), d, SLOT(accept()));

    text.replace('\r', "");
    text.replace(QRegExp("(.)\n +(.)"), "\\1 \\2");
    text.replace(QRegExp("\n - ([^\n]+)"), "\n<li>\\1</li>");
    text.replace(QRegExp(": *\n"), ":\n<ul>\n");
    text.replace(QRegExp("</li>\n\\s*\n"), "</li>\n</ul>\n\n");
    text.replace(QRegExp("\n(\\w[^:\n]+:)"), "\n<p><b>\\1</b></p>");
//    text.replace(QRegExp("<li>([^,.\n]+)([,.] +\\w)"), "<li><b>\\1</b>\\2");
    
    textEdit->setHtml(text);
    textEdit->setReadOnly(true);

    d->setMinimumSize(m_viewManager->scalePixelSize(520),
                      m_viewManager->scalePixelSize(450));
    
    d->exec();

    delete d;
}

QString
MainWindow::getReleaseText() const
{
    bool debug = false;
    QString version = "(unknown version)";

#ifdef BUILD_DEBUG
    debug = true;
#endif // BUILD_DEBUG
#ifdef VECT_VERSION
#ifdef SVNREV
    version = tr("Release %1 : Revision %2").arg(VECT_VERSION).arg(SVNREV);
#else // !SVNREV
    version = tr("Release %1").arg(VECT_VERSION);
#endif // SVNREV
#else // !VECT_VERSION
#ifdef SVNREV
    version = tr("Unreleased : Revision %1").arg(SVNREV);
#endif // SVNREV
#endif // VECT_VERSION

    return tr("%1 : %2 configuration, %3-bit build")
        .arg(version)
        .arg(debug ? tr("Debug") : tr("Release"))
        .arg(sizeof(void *) * 8);
}

void
MainWindow::about()
{
    QString aboutText;

    aboutText += tr("<h3>About Sonic Lineup</h3>");
    aboutText += tr("<p>Sonic Lineup is an application for comparative visualisation and alignment of related audio recordings.<br><a style=\"color: #c1e9f3\" href=\"http://www.sonicvisualiser.org/sonic-lineup/\">http://www.sonicvisualiser.org/sonic-lineup/</a></p>");
    aboutText += QString("<p><small>%1</small></p>").arg(getReleaseText());

    aboutText += 
        tr("<p><small>Sonic Lineup and Sonic Visualiser application code<br>Copyright &copy; 2005&ndash;2019 Chris Cannam"
           " and Queen Mary, University of London.</small></p>");

    aboutText += 
        tr("<p><small>MATCH Audio Alignment plugin<br>Copyright &copy; "
           "2007&ndash;2019 Simon Dixon, Chris Cannam, and Queen Mary "
           "University of London;<br>Copyright &copy; 2014&ndash;2015 Tido "
           "GmbH.</small></p>");

    aboutText += 
        tr("<p><small>NNLS Chroma and Chordino plugin<br>Copyright &copy; "
           "2008&ndash;2019 Matthias Mauch and Queen Mary "
           "University of London.</small></p>");

    aboutText += 
        tr("<p><small>pYIN plugin<br>Copyright &copy; "
           "2012&ndash;2019 Matthias Mauch and Queen Mary "
           "University of London.</small></p>");

    aboutText += 
        tr("<p><small>QM Key Detector plugin<br>Copyright &copy; "
           "2006&ndash;2019 Katy Noland, Christian Landone, and Queen Mary "
           "University of London.</small></p>");

    aboutText += "<p><small>";
    
    aboutText += tr("With Qt v%1 &copy; The Qt Company").arg(QT_VERSION_STR);

    aboutText += "</small><small>";

#ifdef HAVE_JACK
#ifdef JACK_VERSION
    aboutText += tr("<br>With JACK audio output library v%1 &copy; Paul Davis and Jack O'Quin").arg(JACK_VERSION);
#else // !JACK_VERSION
    aboutText += tr("<br>With JACK audio output library &copy; Paul Davis and Jack O'Quin");
#endif // JACK_VERSION
#endif // HAVE_JACK
#ifdef HAVE_PORTAUDIO
    aboutText += tr("<br>With PortAudio audio output library &copy; Ross Bencina and Phil Burk");
#endif // HAVE_PORTAUDIO
#ifdef HAVE_LIBPULSE
#ifdef LIBPULSE_VERSION
    aboutText += tr("<br>With PulseAudio audio output library v%1 &copy; Lennart Poettering and Pierre Ossman").arg(LIBPULSE_VERSION);
#else // !LIBPULSE_VERSION
    aboutText += tr("<br>With PulseAudio audio output library &copy; Lennart Poettering and Pierre Ossman");
#endif // LIBPULSE_VERSION
#endif // HAVE_LIBPULSE
#ifdef HAVE_OGGZ
#ifdef OGGZ_VERSION
    aboutText += tr("<br>With Ogg file decoder (oggz v%1, fishsound v%2) &copy; CSIRO Australia").arg(OGGZ_VERSION).arg(FISHSOUND_VERSION);
#else // !OGGZ_VERSION
    aboutText += tr("<br>With Ogg file decoder &copy; CSIRO Australia");
#endif // OGGZ_VERSION
#endif // HAVE_OGGZ
#ifdef HAVE_OPUS
    aboutText += tr("<br>With Opus decoder &copy; Xiph.Org Foundation");
#endif // HAVE_OPUS
#ifdef HAVE_MAD
#ifdef MAD_VERSION
    aboutText += tr("<br>With MAD mp3 decoder v%1 &copy; Underbit Technologies Inc").arg(MAD_VERSION);
#else // !MAD_VERSION
    aboutText += tr("<br>With MAD mp3 decoder &copy; Underbit Technologies Inc");
#endif // MAD_VERSION
#endif // HAVE_MAD
#ifdef HAVE_SAMPLERATE
#ifdef SAMPLERATE_VERSION
    aboutText += tr("<br>With libsamplerate v%1 &copy; Erik de Castro Lopo").arg(SAMPLERATE_VERSION);
#else // !SAMPLERATE_VERSION
    aboutText += tr("<br>With libsamplerate &copy; Erik de Castro Lopo");
#endif // SAMPLERATE_VERSION
#endif // HAVE_SAMPLERATE
#ifdef HAVE_SNDFILE
#ifdef SNDFILE_VERSION
    aboutText += tr("<br>With libsndfile v%1 &copy; Erik de Castro Lopo").arg(SNDFILE_VERSION);
#else // !SNDFILE_VERSION
    aboutText += tr("<br>With libsndfile &copy; Erik de Castro Lopo");
#endif // SNDFILE_VERSION
#endif // HAVE_SNDFILE
#ifdef HAVE_FFTW3F
#ifdef FFTW3_VERSION
    aboutText += tr("<br>With FFTW3 v%1 &copy; Matteo Frigo and MIT").arg(FFTW3_VERSION);
#else // !FFTW3_VERSION
    aboutText += tr("<br>With FFTW3 &copy; Matteo Frigo and MIT");
#endif // FFTW3_VERSION
#endif // HAVE_FFTW3F
#ifdef HAVE_RUBBERBAND
#ifdef RUBBERBAND_VERSION
    aboutText += tr("<br>With Rubber Band Library v%1 &copy; Particular Programs Ltd").arg(RUBBERBAND_VERSION);
#else // !RUBBERBAND_VERSION
    aboutText += tr("<br>With Rubber Band Library &copy; Particular Programs Ltd");
#endif // RUBBERBAND_VERSION
#endif // HAVE_RUBBERBAND
    aboutText += tr("<br>With Vamp plugin support (API v%1, host SDK v%2) &copy; Chris Cannam and QMUL").arg(VAMP_API_VERSION).arg(VAMP_SDK_VERSION);
#ifdef REDLAND_VERSION
    aboutText += tr("<br>With Redland RDF datastore v%1 &copy; Dave Beckett and the University of Bristol").arg(REDLAND_VERSION);
#else // !REDLAND_VERSION
    aboutText += tr("<br>With Redland RDF datastore &copy; Dave Beckett and the University of Bristol");
#endif // REDLAND_VERSION
    aboutText += tr("<br>With Serd and Sord RDF parser and store &copy; David Robillard");
    aboutText += "</small></p>";

    aboutText += "<p><small>";
    aboutText += tr("Russian UI translation contributed by Alexandre Prokoudine.");
    aboutText += "<br>";
    aboutText += tr("Czech UI translation contributed by Pavel Fric.");
    aboutText += "</small></p>";
    
    aboutText +=
        "<p><small>This program is free software; you can redistribute it and/or "
        "modify it under the terms of the GNU General Public License as "
        "published by the Free Software Foundation; either version 2 of the "
        "License, or (at your option) any later version.<br>See the file "
        "COPYING included with this distribution for more information.</small></p>";

    // use our own dialog so we can influence the size

    QDialog *d = new QDialog(this);

    d->setWindowTitle(tr("About %1").arg(QApplication::applicationName()));
        
    QGridLayout *layout = new QGridLayout;
    d->setLayout(layout);

    int row = 0;
    
    QLabel *iconLabel = new QLabel;
    iconLabel->setPixmap(QApplication::windowIcon().pixmap(64, 64));
    layout->addWidget(iconLabel, row, 0, Qt::AlignTop);

    QLabel *mainText = new QLabel();
    layout->addWidget(mainText, row, 1, 1, 2);

    layout->setRowStretch(row, 10);
    layout->setColumnStretch(1, 10);

    ++row;

    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok);
    layout->addWidget(bb, row++, 0, 1, 3);
    connect(bb, SIGNAL(accepted()), d, SLOT(accept()));

    mainText->setWordWrap(true);
    mainText->setOpenExternalLinks(true);
    mainText->setText(aboutText);

    d->setMinimumSize(m_viewManager->scalePixelSize(420),
                      m_viewManager->scalePixelSize(200));
    
    d->exec();

    delete d;
}

void
MainWindow::keyReference()
{
    m_keyReference->show();
}

void
MainWindow::newerVersionAvailable(QString version)
{
    m_newerVersionIs = version;
    
    //!!! nicer URL would be nicer
    QSettings settings;
    settings.beginGroup("NewerVersionWarning");
    QString tag = QString("version-%1-available-show").arg(version);
    if (settings.value(tag, true).toBool()) {
        QString title(tr("Newer version available"));
        QString text(tr("<h3>Newer version available</h3><p>You are using version %1 of %2, but version %3 is now available.</p><p>Please see the <a style=\"color: #c1e9f3\" href=\"http://www.sonicvisualiser.org/sonic-lineup/\">%4 website</a> for more information.</p>").arg(VECT_VERSION).arg(QApplication::applicationName()).arg(version).arg(QApplication::applicationName()));
        QMessageBox::information(this, title, text);
        settings.setValue(tag, false);
    }
    settings.endGroup();
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



