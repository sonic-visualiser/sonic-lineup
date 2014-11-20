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

#ifndef _PREFERENCES_DIALOG_H_
#define _PREFERENCES_DIALOG_H_

#include <QDialog>

#include "base/Window.h"

class WindowTypeSelector;
class QPushButton;
class QLineEdit;

class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    PreferencesDialog(QWidget *parent = 0);
    virtual ~PreferencesDialog();

public slots:
    void applicationClosing(bool quickly);

protected slots:
    void tuningFrequencyChanged(double freq);
    void tempDirRootChanged(QString root);
    void backgroundModeChanged(int mode);

    void tempDirButtonClicked();

    void okClicked();
    void applyClicked();
    void cancelClicked();

protected:
    QPushButton *m_applyButton;

    QLineEdit *m_tempDirRootEdit;
    
    float m_tuningFrequency;
    QString m_tempDirRoot;
    int m_backgroundMode;

    bool m_changesOnRestart;
};

#endif
