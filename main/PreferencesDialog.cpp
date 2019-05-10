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

#include "PreferencesDialog.h"

#include <QGridLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QString>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QTabWidget>
#include <QLineEdit>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>

#include "widgets/WindowTypeSelector.h"
#include "widgets/IconLoader.h"
#include "base/Preferences.h"

PreferencesDialog::PreferencesDialog(QWidget *parent) :
    QDialog(parent),
    m_changesOnRestart(false)
{
    setWindowTitle(tr("Sonic Vector: Application Preferences"));

    Preferences *prefs = Preferences::getInstance();

    QGridLayout *grid = new QGridLayout;
    setLayout(grid);

    QTabWidget *tab = new QTabWidget;
    grid->addWidget(tab, 0, 0);
    
    tab->setTabPosition(QTabWidget::North);

    // Create this first, as slots that get called from the ctor will
    // refer to it
    m_applyButton = new QPushButton(tr("Apply"));

    // Create all the preference widgets first, then create the
    // individual tab widgets and place the preferences in their
    // appropriate places in one go afterwards

    m_tuningFrequency = prefs->getTuningFrequency();

    QDoubleSpinBox *frequency = new QDoubleSpinBox;
    frequency->setMinimum(100.0);
    frequency->setMaximum(5000.0);
    frequency->setSuffix(" Hz");
    frequency->setSingleStep(1);
    frequency->setValue(m_tuningFrequency);
    frequency->setDecimals(2);

    connect(frequency, SIGNAL(valueChanged(double)),
            this, SLOT(tuningFrequencyChanged(double)));

    QSettings settings;
    settings.beginGroup("Preferences");
    m_useAlignmentProgram = settings.value("use-external-alignment", false).toBool();
    QString program = settings.value("external-alignment-program", "").toString();
    m_alignmentProgram = program;
    program.replace("$HOME", tr("<home directory>"));
    settings.endGroup();

    m_alignmentProgramToggle = new QCheckBox();
    connect(m_alignmentProgramToggle, SIGNAL(clicked()),
            this, SLOT(alignmentProgramToggleClicked()));
    m_alignmentProgramToggle->setChecked(m_useAlignmentProgram);
    m_alignmentProgramEdit = new QLineEdit;
    m_alignmentProgramEdit->setText(program);
    m_alignmentProgramEdit->setReadOnly(true);
    m_alignmentProgramEdit->setEnabled(m_useAlignmentProgram);
    m_alignmentProgramButton = new QPushButton();
    m_alignmentProgramButton->setIcon(IconLoader().load("fileopen"));
    connect(m_alignmentProgramButton, SIGNAL(clicked()),
            this, SLOT(alignmentProgramButtonClicked()));
    m_alignmentProgramButton->setFixedSize(QSize(24, 24));
    m_alignmentProgramButton->setEnabled(m_useAlignmentProgram);

    m_normaliseAudio = prefs->getNormaliseAudio();
    m_normaliseAudioToggle = new QCheckBox();
    m_normaliseAudioToggle->setChecked(m_normaliseAudio);
    connect(m_normaliseAudioToggle, SIGNAL(clicked()),
            this, SLOT(normaliseAudioToggleClicked()));
    
    QFrame *frame = new QFrame;
    
    QGridLayout *subgrid = new QGridLayout;
    frame->setLayout(subgrid);

    int row = 0;

    subgrid->addWidget(new QLabel(tr("%1:").arg(prefs->getPropertyLabel
                                                ("Tuning Frequency"))),
                       row, 0);
    subgrid->addWidget(frequency, row++, 2, 1, 1);

    subgrid->addWidget(new QLabel(tr("%1:").arg(tr("Use external alignment program"))),
                       row, 0);
    subgrid->addWidget(m_alignmentProgramToggle, row, 1, 1, 1);
    subgrid->addWidget(m_alignmentProgramEdit, row, 2, 1, 1);
    subgrid->addWidget(m_alignmentProgramButton, row, 3, 1, 1);
    row++;

    subgrid->addWidget(new QLabel(tr("%1:").arg(tr("Normalise audio"))), row, 0);
    subgrid->addWidget(m_normaliseAudioToggle, row, 1, 1, 1);
    row++;
    
    subgrid->setColumnStretch(2, 10);
    subgrid->setRowStretch(row, 10);
    
    tab->addTab(frame, tr("&General"));

    QDialogButtonBox *bb = new QDialogButtonBox(Qt::Horizontal);
    grid->addWidget(bb, 1, 0);
    
    QPushButton *ok = new QPushButton(tr("OK"));
    QPushButton *cancel = new QPushButton(tr("Cancel"));
    bb->addButton(ok, QDialogButtonBox::AcceptRole);
    bb->addButton(m_applyButton, QDialogButtonBox::ApplyRole);
    bb->addButton(cancel, QDialogButtonBox::RejectRole);
    connect(ok, SIGNAL(clicked()), this, SLOT(okClicked()));
    connect(m_applyButton, SIGNAL(clicked()), this, SLOT(applyClicked()));
    connect(cancel, SIGNAL(clicked()), this, SLOT(cancelClicked()));

    m_applyButton->setEnabled(false);
}

PreferencesDialog::~PreferencesDialog()
{
    std::cerr << "PreferencesDialog::~PreferencesDialog()" << std::endl;
}

void
PreferencesDialog::tuningFrequencyChanged(double freq)
{
    m_tuningFrequency = freq;
    m_applyButton->setEnabled(true);
}

void
PreferencesDialog::alignmentProgramToggleClicked()
{
    m_useAlignmentProgram = m_alignmentProgramToggle->isChecked();
    m_alignmentProgramEdit->setEnabled(m_useAlignmentProgram);
    m_alignmentProgramButton->setEnabled(m_useAlignmentProgram);
    m_applyButton->setEnabled(true);
    m_changesOnRestart = true;
}
    
void
PreferencesDialog::alignmentProgramButtonClicked()
{
    QString file = QFileDialog::getOpenFileName
        (this, tr("Select an external alignment program to run"),
         m_alignmentProgram);
    if (file == "") return;
    m_alignmentProgram = file;
    file.replace("$HOME", tr("<home directory>"));
    m_alignmentProgramEdit->setText(file);
    m_applyButton->setEnabled(true);
    m_changesOnRestart = true;
}

void
PreferencesDialog::normaliseAudioToggleClicked()
{
    m_normaliseAudio = m_normaliseAudioToggle->isChecked();
    m_applyButton->setEnabled(true);
    m_changesOnRestart = true;
}

void
PreferencesDialog::okClicked()
{
    applyClicked();
    accept();
}

void
PreferencesDialog::applyClicked()
{
    Preferences *prefs = Preferences::getInstance();
    prefs->setTuningFrequency(m_tuningFrequency);
    prefs->setNormaliseAudio(m_normaliseAudio);

    QSettings settings;
    settings.beginGroup("Preferences");
    settings.setValue("use-external-alignment", m_useAlignmentProgram);
    settings.setValue("external-alignment-program", m_alignmentProgram);
    settings.endGroup();
    
    m_applyButton->setEnabled(false);

    if (m_changesOnRestart) {
        QMessageBox::information(this, tr("Preferences"),
                                 tr("One or more of the application preferences you have changed may not take full effect until Sonic Vector is restarted.\nPlease exit and restart the application now if you want these changes to take effect immediately."));
        m_changesOnRestart = false;
    }
}    

void
PreferencesDialog::cancelClicked()
{
    reject();
}

void
PreferencesDialog::applicationClosing(bool quickly)
{
    if (quickly) {
        reject();
        return;
    }

    if (m_applyButton->isEnabled()) {
        int rv = QMessageBox::warning
            (this, tr("Preferences Changed"),
             tr("Some preferences have been changed but not applied.\n"
                "Apply them before closing?"),
             QMessageBox::Apply | QMessageBox::Discard,
             QMessageBox::Discard);
        if (rv == QMessageBox::Apply) {
            applyClicked();
            accept();
        } else {
            reject();
        }
    } else {
        accept();
    }
}

