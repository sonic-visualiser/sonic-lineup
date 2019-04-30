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

#ifndef _MAIN_WINDOW_H_
#define _MAIN_WINDOW_H_

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
#include <map>

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

class MainWindow : public MainWindowBase
{
    Q_OBJECT

public:
    MainWindow(bool withAudioOutput = true);
    virtual ~MainWindow();

public slots:
    virtual void preferenceChanged(PropertyContainer::PropertyName);
    virtual bool commitData(bool mayAskUser); // on session shutdown

    void goFullScreen();
    void endFullScreen();

    void selectMainPane();

protected slots:
    virtual void openFiles();
    virtual void openLocation();
    virtual void openRecentFile();
    virtual void saveSession();
    virtual void saveSessionAs();
    virtual void newSession();
    virtual void closeSession();
    virtual void preferences();

    virtual void curveModeSelected();
    virtual void waveformModeSelected();
    virtual void spectrogramModeSelected();
    virtual void melodogramModeSelected();

    virtual void toggleSalientFeatures();

    virtual void renameCurrentLayer();

    virtual void paneAdded(Pane *);
    virtual void paneHidden(Pane *);
    virtual void paneAboutToBeDeleted(Pane *);
    virtual void paneDropAccepted(Pane *, QStringList);
    virtual void paneDropAccepted(Pane *, QString);

    virtual void alignToggled();
    virtual void playSpeedChanged(int);

    virtual void speedUpPlayback();
    virtual void slowDownPlayback();
    virtual void restoreNormalPlayback();

    virtual void monitoringLevelsChanged(float, float);
    
    virtual void sampleRateMismatch(sv_samplerate_t, sv_samplerate_t, bool);
    virtual void audioOverloadPluginDisabled();
    virtual void audioTimeStretchMultiChannelDisabled();

    virtual void documentModified();
    virtual void documentRestored();

    virtual void updateMenuStates();
    virtual void updateDescriptionLabel();

    virtual void layerRemoved(Layer *);
    virtual void layerInAView(Layer *, bool);

    virtual void mainModelChanged(WaveFileModel *);
    virtual void mainModelGainChanged(float);
    virtual void mainModelPanChanged(float);
    virtual void modelAdded(Model *);
    virtual void modelAboutToBeDeleted(Model *);

    virtual void modelGenerationFailed(QString, QString);
    virtual void modelGenerationWarning(QString, QString);
    virtual void modelRegenerationFailed(QString, QString, QString);
    virtual void modelRegenerationWarning(QString, QString, QString);

    virtual void alignmentComplete(AlignmentModel *);
    virtual void alignmentFailed(QString);
    virtual void salientLayerCompletionChanged();

    virtual void rightButtonMenuRequested(Pane *, QPoint point);

    virtual void setupRecentFilesMenu();

    virtual void showLayerTree();

    virtual void handleOSCMessage(const OSCMessage &);

    virtual void mouseEnteredWidget();
    virtual void mouseLeftWidget();

    virtual void website();
    virtual void help();
    virtual void about();
    virtual void keyReference();

protected:
    LevelPanToolButton      *m_mainLevelPan;
    AudioDial               *m_playSpeed;
    
    QScrollArea             *m_mainScroll;

    bool                     m_mainMenusCreated;
    QMenu                   *m_playbackMenu;
    QMenu                   *m_recentFilesMenu;
    QMenu                   *m_rightButtonMenu;
    QMenu                   *m_rightButtonPlaybackMenu;

    QAction                 *m_deleteSelectedAction;
    QAction                 *m_ffwdAction;
    QAction                 *m_rwdAction;

    QAction                 *m_playAction;
    QAction                 *m_zoomInAction;
    QAction                 *m_zoomOutAction;
    QAction                 *m_zoomFitAction;
    QAction                 *m_scrollLeftAction;
    QAction                 *m_scrollRightAction;
    QAction                 *m_showPropertyBoxesAction;

    bool                     m_exiting;

    QFrame                  *m_playControlsSpacer;
    int                      m_playControlsWidth;

    QPointer<PreferencesDialog> m_preferencesDialog;
    QPointer<QTreeView>      m_layerTreeView;

    KeyReference            *m_keyReference;

    typedef std::set<Layer *> LayerSet;
    typedef std::map<Pane *, LayerSet> PaneLayerMap;
    PaneLayerMap             m_hiddenLayers;

    virtual void setupMenus();
    virtual void setupFileMenu();
    virtual void setupEditMenu();
    virtual void setupViewMenu();
    virtual void setupHelpMenu();
    virtual void setupToolbars();

    enum DisplayMode {
        CurveMode,
        WaveformMode,
        SpectrogramMode,
        MelodogramMode
    };
    virtual void reselectMode();
    DisplayMode m_displayMode;

    virtual void closeEvent(QCloseEvent *e);
    bool checkSaveModified();

    virtual void configureNewPane(Pane *p);
    virtual bool selectExistingLayerForMode(Pane *, QString,
                                            Model **createFrom);

    virtual void addSalientFeatureLayer(Pane *, WaveFileModel *);
    virtual void mapSalientFeatureLayer(AlignmentModel *);

    // Return the salient-feature layer in the given pane. If pane is
    // unspecified, return the main salient-feature layer, i.e. the
    // first one found in any pane that is associated with the main
    // model. If none is found, return nullptr.
    virtual TimeInstantLayer *findSalientFeatureLayer(Pane *pane = nullptr);
    
    bool m_salientCalculating;
    std::set<AlignmentModel *> m_salientPending;
    int m_salientColour;
    
    virtual void updateVisibleRangeDisplay(Pane *p) const;
    virtual void updatePositionStatusDisplays() const;

    void loadStyle();
};


#endif
