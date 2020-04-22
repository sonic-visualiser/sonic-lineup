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

#ifndef VECT_MAIN_WINDOW_H
#define VECT_MAIN_WINDOW_H

#include <QFrame>
#include <QString>
#include <QUrl>
#include <QMainWindow>
#include <QPointer>

#include "framework/MainWindowBase.h"
#include "base/Command.h"
#include "view/ViewManager.h"
#include "base/PropertyContainer.h"
#include "base/RecentFiles.h"
#include "layer/LayerFactory.h"
#include "transform/Transform.h"
#include "framework/SVFileReader.h"
#include "widgets/InteractiveFileFinder.h"

#include "SmallSession.h"

#include <map>

class VersionTester;
class Document;
class PaneStack;
class Pane;
class View;
class Fader;
class Overview;
class Layer;
class WaveformLayer;
class TimeInstantLayer;
class WaveFileModel;
class AudioCallbackPlaySource;
class AudioCallbackPlayTarget;
class CommandHistory;
class QMenu;
class AudioDial;
class QLabel;
class QCheckBox;
class PreferencesDialog;
class QTreeView;
class QPushButton;
class KeyReference;
class QScrollArea;
class OSCMessage;
class QToolButton;

class MainWindow : public MainWindowBase
{
    Q_OBJECT

public:
    MainWindow(AudioMode audioMode);
    virtual ~MainWindow();

signals:
    void canSelectPreviousDisplayMode(bool);
    void canSelectNextDisplayMode(bool);

public slots:
    void openSmallSession(const SmallSession &);

    void preferenceChanged(PropertyContainer::PropertyName) override;
    bool commitData(bool mayAskUser); // on session shutdown

    void goFullScreen();
    void endFullScreen();

    void selectMainPane();

protected slots:
    virtual void openFiles();
    virtual void openLocation();
    virtual void openRecentSession();
    virtual void checkpointSession();
    virtual void browseRecordedAudio();
    virtual void newSession();
    virtual void preferences();

    bool reopenLastSession();
    void closeSession() override;

    void outlineWaveformModeSelected();
    void standardWaveformModeSelected();
    void spectrogramModeSelected();
    void melodogramModeSelected();
    void azimuthModeSelected();
    void pitchModeSelected();
    void keyModeSelected();

    void previousDisplayMode();
    void nextDisplayMode();

    void toggleSalientFeatures();
    void toggleVerticalScales();

    virtual void renameCurrentLayer();

    void paneAdded(Pane *) override;
    void paneHidden(Pane *) override;
    void paneAboutToBeDeleted(Pane *) override;
    void paneDropAccepted(Pane *, QStringList) override;
    void paneDropAccepted(Pane *, QString) override;

    void record() override;
    
    virtual void alignToggled();
    virtual void tuningDifferenceToggled();
    virtual void playSpeedChanged(int);

    virtual void speedUpPlayback();
    virtual void slowDownPlayback();
    virtual void restoreNormalPlayback();

    void monitoringLevelsChanged(float, float) override;

    void introDialog();
    void checkForNewerVersion();
    
    void betaReleaseWarning();

    void sampleRateMismatch(sv_samplerate_t, sv_samplerate_t, bool) override;
    void audioOverloadPluginDisabled() override;

    void documentModified() override;
    void documentRestored() override;

    void updateMenuStates() override;
    void updateDescriptionLabel() override;
    void updateWindowTitle() override;

    void layerRemoved(Layer *) override;
    void layerInAView(Layer *, bool) override;

    void mainModelChanged(ModelId) override;
    void modelAdded(ModelId) override;

    virtual void mainModelGainChanged(float);
    virtual void mainModelPanChanged(float);

    void modelGenerationFailed(QString, QString) override;
    void modelGenerationWarning(QString, QString) override;
    void modelRegenerationFailed(QString, QString, QString) override;
    void modelRegenerationWarning(QString, QString, QString) override;

    void alignmentComplete(ModelId) override;
    void alignmentFailed(QString) override;

    virtual void salientLayerCompletionChanged(ModelId);

    void paneRightButtonMenuRequested(Pane *, QPoint) override { /* none */ }
    void panePropertiesRightButtonMenuRequested(Pane *, QPoint) override { /* none */ }
    void layerPropertiesRightButtonMenuRequested(Pane *, Layer *, QPoint) override { /* none */ }

    virtual void setupRecentSessionsMenu();

    virtual void showLayerTree();

    void handleOSCMessage(const OSCMessage &) override;

    virtual void mouseEnteredWidget();
    virtual void mouseLeftWidget();

    virtual void website();
    virtual void help();
    virtual void whatsNew();
    virtual void about();
    virtual void keyReference();

    void newerVersionAvailable(QString) override;

protected:
    LevelPanToolButton      *m_mainLevelPan;
    AudioDial               *m_playSpeed;
    
    QScrollArea             *m_mainScroll;

    bool                     m_mainMenusCreated;
    QMenu                   *m_playbackMenu;
    QMenu                   *m_recentSessionsMenu;

    QAction                 *m_deleteSelectedAction;
    QAction                 *m_ffwdAction;
    QAction                 *m_rwdAction;

    QAction                 *m_playAction;
    QAction                 *m_zoomInAction;
    QAction                 *m_zoomOutAction;
    QAction                 *m_zoomFitAction;
    QAction                 *m_scrollLeftAction;
    QAction                 *m_scrollRightAction;

    QAction                 *m_selectPreviousPaneAction;
    QAction                 *m_selectNextPaneAction;
    QAction                 *m_selectPreviousDisplayModeAction;
    QAction                 *m_selectNextDisplayModeAction;

    RecentFiles              m_recentSessions;
    
    bool                     m_exiting;

    QFrame                  *m_playControlsSpacer;
    int                      m_playControlsWidth;

    QPointer<PreferencesDialog> m_preferencesDialog;
    QPointer<QTreeView>      m_layerTreeView;

    KeyReference            *m_keyReference;
    VersionTester           *m_versionTester;
    bool                     m_networkPermission;
    QString                  m_newerVersionIs;

    QString getReleaseText() const;
    
    void setupMenus() override;
    
    virtual void setupFileMenu();
    virtual void setupViewMenu();
    virtual void setupHelpMenu();
    virtual void setupToolbars();

    enum DisplayMode {
        OutlineWaveformMode,
        WaveformMode,
        SpectrogramMode,
        MelodogramMode,
        AzimuthMode,
        PitchMode,
        KeyMode
    };
    std::map<DisplayMode, QPushButton *> m_modeButtons;
    std::map<DisplayMode, QString> m_modeLayerNames;
    std::vector<DisplayMode> m_modeDisplayOrder;
    
    virtual void reselectMode();
    virtual void updateModeFromLayers(); // after loading a session
    virtual void selectTransformDrivenMode(DisplayMode mode,
                                           QString transformId,
                                           Transform::ParameterMap parameters,
                                           QString layerPropertyXml,
                                           bool includeGhostReference);
    DisplayMode m_displayMode;

    void closeEvent(QCloseEvent *e) override;
    bool checkSaveModified() override;

    virtual void configureNewPane(Pane *p);
    virtual bool selectExistingLayerForMode(Pane *, QString,
                                            ModelId *createFrom);

    virtual void addSalientFeatureLayer(Pane *, ModelId); // a WaveFileModel
    virtual void mapSalientFeatureLayer(ModelId); // an AlignmentModel

    // Return the salient-feature layer in the given pane. If pane is
    // unspecified, return the main salient-feature layer, i.e. the
    // first one found in any pane that is associated with the main
    // model. If none is found, return nullptr.
    virtual TimeInstantLayer *findSalientFeatureLayer(Pane *pane = nullptr);
    
    bool m_salientCalculating;
    std::set<ModelId> m_salientPending; // AlignmentModels
    int m_salientColour;
    
    void updateVisibleRangeDisplay(Pane *p) const override;
    void updatePositionStatusDisplays() const override;

    // Generate and return a filename into which to save the session,
    // based on the identity of the main model. This should be fixed
    // from the point where the identity of the main model is first
    // known.
    QString makeSessionFilename();

    // Generate and return a label to associate with the session in
    // the recent-sessions menu. This may depend on factors other than
    // the identity of the main model, such as the number of files
    // open, and so may change during the lifetime of the session.
    QString makeSessionLabel();

    // Construct a SmallSession, which contains the origin URIs of the
    // files in the current session
    SmallSession makeSmallSession();

    // Open a session from the given SmallSession file path
    void openSmallSessionFile(QString path);
    
    enum SessionState {
        NoSession,
        SessionLoading,
        SessionActive
    };
    SessionState m_sessionState;

    void loadStyle();
};


#endif
