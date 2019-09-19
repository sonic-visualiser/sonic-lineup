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
#include <QFrame>
#include <QApplication>

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
        { d.tr("Welcome!"),
          d.tr("<p>The first thing is to open some audio files. The Open button<br>"
               "is on the toolbar up here.</p>"
               "<p>(But please read these introductory notes first!)</p>"
               "<p>If you have more than one recording of the same thing,<br>"
               "such as multiple performances, takes, or even cover versions,"
               "<br>try opening them all in one go.</p>"
               "<p>Comparative visualisation is what this app is designed for.</p>"
               "<p>You can also record a new track directly from the "
               "microphone.</p>")
        },
        { d.tr("Scroll, zoom, and play"),
          d.tr("<p>Your audio files will all appear in this main pane.</p>"
               "<p>Drag left and right to move through time, and use<br>"
               "two-finger scroll-drag, or a scroll wheel, to zoom.</p>"
               "<p>You can also move using the left and right cursor keys,<br>"
               "and zoom using the up and down keys.</p>"
               "<p>%1 will try to align the audio files, so as to<br>"
               "ensure they scroll together in terms of musical material.<br>"
               "You can toggle or control alignment in the Playback menu.</p>"
              ).arg(QApplication::applicationName())
        },
        { d.tr("Change your view"),
          d.tr("<p>Use the buttons along the bottom to change the current view.</p>"
               "<p>There are two waveform views: Outline for a simplified<br>"
               "overview, or a more typical waveform for detail. And two<br>"
               "spectrograms with different frequency and colour profiles.</p>"
               "<p>The Sung Pitch view shows pitch profiles, in the case of<br>"
               "solo singing or similar music; Key is a key-likelihood plot;<br>"
               "and Stereo Azimuth shows a decomposition of the stereo plan.</p>"
               "<p>See the documentation in the Help menu for more details.</p>"
               "</ul>"
              )
        }
    };

    QGridLayout *outerLayout = new QGridLayout;
    d.setLayout(outerLayout);
    QFrame *outerFrame = new QFrame;
#ifdef Q_OS_WIN32
    outerFrame->setFrameStyle(QFrame::Panel | QFrame::Raised);
#endif
    outerLayout->addWidget(outerFrame, 0, 0);
    
    QGridLayout *layout = new QGridLayout;
    outerFrame->setLayout(layout);
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

    int dpratio = d.devicePixelRatio();
    int sz = dpratio * int(round(parent->height() * 0.1));

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
        arrowPixmaps[i].setDevicePixelRatio(dpratio);
    }

    vector<QLabel *> arrowLabels;

    for (auto p: arrowPixmaps) {
        QLabel *arrowLabel = new QLabel;
        arrowLabel->setPixmap(p);
        arrowLabel->setVisible(arrowLabels.empty());
        arrowLabels.push_back(arrowLabel);
    }
        
    layout->addWidget(arrowLabels[0], 0, 0);
    layout->addWidget(arrowLabels[1], 1, 0, 1, 1, Qt::AlignTop);
    layout->addWidget(arrowLabels[2], 1, 0, 1, 1, Qt::AlignBottom);

    QFont smallerFont(d.font());
#ifdef Q_OS_WIN32
    if (smallerFont.pixelSize() > 0) {
        smallerFont.setPixelSize(int(ceil(smallerFont.pixelSize() * 1.1)));
    } else {
        smallerFont.setPointSize(int(ceil(smallerFont.pointSize() * 1.1)));
    }
#endif
    
    QFont biggerFont(d.font());
    if (biggerFont.pixelSize() > 0) {
        biggerFont.setPixelSize(int(ceil(biggerFont.pixelSize() * 1.4)));
    } else {
        biggerFont.setPointSize(int(ceil(biggerFont.pointSize() * 1.4)));
    }
    
    QLabel *numberLabel = new QLabel;
    numberLabel->setText(d.tr("%1.").arg(page));
    layout->addWidget(numberLabel, 0, 1, 1, 1,
                      Qt::AlignRight | Qt::AlignBottom);
    numberLabel->setFont(biggerFont);

    QLabel *titleLabel = new QLabel;
    titleLabel->setText(texts[page-1].first);
    titleLabel->setFont(biggerFont);
    layout->addWidget(titleLabel, 0, 2, 1, 1,
                      Qt::AlignLeft | Qt::AlignBottom);

    QLabel *bodyLabel = new QLabel;
    bodyLabel->setWordWrap(false);
    bodyLabel->setText(texts[page-1].second);
    bodyLabel->setFont(smallerFont);
    layout->addWidget(bodyLabel, 1, 2);
        
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
    close->setIcon(IconLoader().load("cross"));
    close->setEnabled(false);
    layout->addWidget(bb, 3, 0, 1, 3);

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
    
    d.setMinimumSize(QSize(int(ceil(parent->width() * 0.4)),
                           int(ceil(parent->height() * 0.6))));
    d.setModal(false);
    d.exec();
    
    settings.setValue("shown", true);
}

