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

#include "IntroDialog.h"

#include <QSettings>
#include <QDialog>
#include <QGridLayout>
#include <QLabel>
#include <QSvgWidget>
#include <QSvgRenderer>
#include <QPainter>
#include <QDialogButtonBox>
#include <QPushButton>

#include "widgets/IconLoader.h"

#include <vector>
#include <cmath>

using namespace std;

IntroDialog::IntroDialog(QWidget *parent)
{
    QSettings settings;
    settings.beginGroup("IntroDialog");
    if (settings.value("shown", false).toBool()) {
        return;
    }

    QDialog d(parent, Qt::SplashScreen);
    d.setWindowTitle(d.tr("Welcome!"));

    vector<pair<QString, QString>> texts {
        { d.tr("Open some audio files!"),
          d.tr("<p>If you have more than one recording of the same thing,<br>"
               "such as multiple performances, takes, or even cover versions,"
               "<br>try opening them all in one go.</p>"
               "<p>Comparative visualisation is what this app is all about!</p>"
               "<p>You can also record a new track directly from the "
               "microphone.</p>")
        },
        { d.tr("Scroll, zoom, and play"),
          d.tr("<p>Drag left and right in the main pane to move through time,<br>"
               "and use two-finger scroll-drag, or a scroll wheel, to zoom.</p>"
               "<p>You can also move using the left and right cursor keys,<br>"
               "and zoom using the up and down keys.</p>"
               "<p>Sonic Lineup will try to align the audio files so as to<br>"
               "ensure they scroll in sync in terms of musical material.<br>"
               "You can switch off or control alignment in the Playback menu.</p>"
              )
        },
        { d.tr("Switch view"),
          d.tr("<p>Use the buttons along the bottom to change the current view:</p>"
               "<ul><li>Outline waveform: simplified waveform on a single axis</li>"
               "<li>Waveform: classic audio-editor style waveform</li>"
               "<li>Melodic spectrogram: detailed log-frequency spectrogram in limited range</li>"
               "<li>Spectrogram: full-range dB spectrogram</li>"
               "<li>Sung pitch: pitch contour extracted as if from singing</li>"
               "<li>Key: likelihood plot for key within circle-of-fifths</li>"
               "<li>Stereo azimuth: left/right decomposition of frequency bins</li>"
               "</ul>"
              )
        }
    };
    
    QGridLayout *layout = new QGridLayout;
    d.setLayout(layout);
    layout->setRowStretch(0, 10);
    layout->setRowStretch(1, 20);
    layout->setRowStretch(2, 10);
    layout->setRowStretch(3, 0);
    layout->setColumnStretch(0, 0);
    layout->setColumnStretch(1, 0);
    layout->setColumnStretch(2, 20);

    int page = 1;

    vector<QString> arrowFiles {
        ":icons/scalable/arrow-above-white.svg",
        ":icons/scalable/arrow-left-white.svg",
        ":icons/scalable/arrow-below-white.svg"
    };

    int sz = int(round(parent->height() * 0.1));

    vector<QPixmap> arrowPixmaps {
        QPixmap(sz, sz),
        QPixmap(sz, (sz * 6) / 10),
        QPixmap(sz, sz)
    };

    for (int i = 0; i < int(arrowFiles.size()); ++i) {
        arrowPixmaps[i].fill(Qt::transparent);
        QPainter painter(&arrowPixmaps[i]);
        QSvgRenderer renderer(arrowFiles[i]);
        renderer.render(&painter);
    }

    vector<QLabel *> arrowLabels;

    for (auto p: arrowPixmaps) {
        QLabel *arrowLabel = new QLabel;
        arrowLabel->setPixmap(p);
        arrowLabel->setVisible(arrowLabels.empty());
        arrowLabels.push_back(arrowLabel);
    }
        
    layout->addWidget(arrowLabels[0], 0, 0);
    layout->addWidget(arrowLabels[1], 1, 0);
    layout->addWidget(arrowLabels[2], 1, 0, 1, 1, Qt::AlignBottom);

    QLabel *numberLabel = new QLabel;
    numberLabel->setText(d.tr("%1.").arg(page));
    layout->addWidget(numberLabel, 0, 1, 1, 1,
                      Qt::AlignRight | Qt::AlignBottom);
    QFont f(numberLabel->font());
    if (f.pixelSize() > 0) {
        f.setPixelSize(int(ceil(f.pixelSize() * 1.4)));
    } else {
        f.setPointSize(int(ceil(f.pointSize() * 1.4)));
    }
    numberLabel->setFont(f);

    QLabel *titleLabel = new QLabel;
    titleLabel->setText(texts[page-1].first);
    titleLabel->setFont(f);
    layout->addWidget(titleLabel, 0, 2, 1, 1,
                      Qt::AlignLeft | Qt::AlignBottom);

    QLabel *bodyLabel = new QLabel;
    bodyLabel->setWordWrap(false);
    bodyLabel->setText(texts[page-1].second);
    layout->addWidget(bodyLabel, 1, 2);
    
    d.setMinimumSize(parent->size() * 0.6);
        
    QDialogButtonBox *bb = new QDialogButtonBox;

    QPushButton *prev = bb->addButton
        (d.tr("&Prev"), QDialogButtonBox::ActionRole);
    prev->setIcon(IconLoader().load("rewind"));
    prev->setEnabled(false);

    QPushButton *next = bb->addButton
        (d.tr("&Next"), QDialogButtonBox::ActionRole);
    next->setIcon(IconLoader().load("ffwd"));
    
    QPushButton *close = bb->addButton(QDialogButtonBox::Close);
    QObject::connect(close, SIGNAL(clicked()), &d, SLOT(accept()));
    close->setEnabled(false);
    layout->addWidget(bb, 3, 0);

    auto repage =
        [&](int step)
        {
            arrowLabels[page-1]->hide();

            int npages = int(texts.size());
            page += step;

            prev->setEnabled(page > 1);

            next->setEnabled(page < npages);
            next->setDefault(page < npages);

            close->setEnabled(page == npages);
            close->setDefault(page == npages);

            numberLabel->setText(d.tr("%1.").arg(page));
            titleLabel->setText(texts[page-1].first);
            bodyLabel->setText(texts[page-1].second);

            arrowLabels[page-1]->show();
        };

    QObject::connect(next, &QPushButton::clicked, [&]() { repage(1); });
    QObject::connect(prev, &QPushButton::clicked, [&]() { repage(-1); });

    d.exec();
    
    settings.setValue("shown", true);
}

