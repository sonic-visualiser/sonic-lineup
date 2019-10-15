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

#include "NetworkPermissionTester.h"

#include "../version.h"

#include <QWidget>
#include <QString>
#include <QSettings>
#include <QCoreApplication>
#include <QDialog>
#include <QGridLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QCheckBox>

bool
NetworkPermissionTester::havePermission()
{
    QSettings settings;
    settings.beginGroup("Preferences");
    
    QString tag = QString("network-permission-%1").arg(VECT_VERSION);

    bool permish = false;

    if (settings.contains(tag)) {
	permish = settings.value(tag, false).toBool();
    } else {

	QDialog d;
	d.setWindowTitle(QCoreApplication::translate
                         ("NetworkPermissionTester", 
                          "Sonic Lineup"));

	QGridLayout *layout = new QGridLayout;
	d.setLayout(layout);

	QLabel *label = new QLabel;
	label->setWordWrap(true);
	label->setText
	    (QCoreApplication::translate
	     ("NetworkPermissionTester",
	      "<img src=\":icons/qm-logo-smaller-white.png\" style=\"float:right; margin-left: 4em\"><h2>Welcome to Sonic Lineup!</h2>"
	      "<p>Sonic Lineup is a program for comparative visualisation and alignment of groups of related audio recordings.</p>"
	      "<p>Developed in the Centre for Digital Music at Queen Mary, University of London, Sonic Lineup is open source software under the GNU General Public License.</p>"
              "<p><hr></p>"
	      "<p><b>Before we go on...</b></p>"
	      "<p>Sonic Lineup would like to make occasional network requests, to check for updates.</li></ul>"
	      "<p>No personal information is sent, but aggregated counts of requests may be made for usage reporting.</p>"
	      "<p>We recommend that you allow this, because it makes Sonic Lineup more useful to you and supports the public funding of this work. But if you do not wish to do so, please un-check the box below.<br></p>"));
	layout->addWidget(label, 0, 0);

	QCheckBox *cb = new QCheckBox(QCoreApplication::translate("NetworkPermissionTester", "Allow this"));
	cb->setChecked(true);
	layout->addWidget(cb, 1, 0);
	
	QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Ok);
	QObject::connect(bb, SIGNAL(accepted()), &d, SLOT(accept()));
	layout->addWidget(bb, 2, 0);
	
	d.exec();

	permish = cb->isChecked();
	settings.setValue(tag, permish);
    }

    settings.endGroup();

    return permish;
}

   

