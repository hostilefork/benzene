//
// rundialog.h
// This file is part of Benzene
// Copyright (C) 2002-2014 HostileFork.com
//
// Benzene is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Benzene is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Benzene.  If not, see <http://www.gnu.org/licenses/>.
//
// See http://benzene.hostilefork.com/ for more information on this project
//

#include <QtWidgets>

#include "rundialog.h"

namespace benzene {

RunDialog::RunDialog (QWidget *parent)
	: QDialog (parent)
{
    _label = new QLabel("(progress string here)");
    _progress = new QProgressBar;
    _label->setBuddy(_progress);

    // Show as "busy" for now, no information on how long command will take
    // http://doc.qt.digia.com/4.6/qprogressbar.html#details
    _progress->setMinimum(0);
    _progress->setMaximum(0);

    _cancelButton = new QPushButton("&Cancel");
    _terminateButton = new QPushButton("&Terminate");
    _terminateButton->setEnabled(false);

    _buttonBox = new QDialogButtonBox(Qt::Horizontal);
    _buttonBox->addButton(_cancelButton, QDialogButtonBox::ActionRole);
    _buttonBox->addButton(_terminateButton, QDialogButtonBox::ActionRole);

    // REVIEW: is clicked  the right event?
    // http://doc.trolltech.com/4.5/qabstractbutton.html#clicked
    connect(
        _cancelButton, &QPushButton::clicked,
        this, &RunDialog::canceled,
        Qt::DirectConnection
    );

    connect(
        _terminateButton, &QPushButton::clicked,
        this, &RunDialog::terminated,
        Qt::DirectConnection
    );

    QVBoxLayout * mainLayout = new QVBoxLayout;
    mainLayout->setSizeConstraint(QLayout::SetFixedSize);
    mainLayout->addWidget(_label);
    mainLayout->addWidget(_progress);
    mainLayout->addWidget(_buttonBox);
    setLayout(mainLayout);

    setWindowModality(Qt::WindowModal);

    startTimer(1000);

    // Hiding the close button doesn't seem to be an easy option; the
    // given techniques do not work, or cause erratic positioning.  So
    // instead we should give the close button behavior...or maybe
    // implement a custom dialog?
    //
    // http://stackoverflow.com/questions/16920412/
}
 

void RunDialog::setProgressString (QString message) {
    _label->setText(message);
}


void RunDialog::timerEvent (QTimerEvent * event) {
    if (not _tickShown) {
        QElapsedTimer timer;
        timer.start();
        _tickShown = timer.msecsSinceReference();
        show();
    } else {
        emit okayToClose();
    }
    killTimer(event->timerId());
}


void RunDialog::requestClose () {

    if (not _tickShown) {
        // If we never showed the dialog in the first place, there's no need
        // to "debounce" it by holding it up a little longer...
        emit okayToClose();
        return;
    }

    QElapsedTimer timer;
    timer.start();
    qint64 delta = timer.msecsSinceReference() - *_tickShown;

    if (delta > 1000) {
        // If the dialog was shown, and has been up for at least a second,
        // that's long enough.
        emit okayToClose();
        return;
    }

    // Set a timer to emit okayToClose() once we've had the shown dialog up
    // for a full second.
    startTimer(1000 - delta);
}


RunDialog::~RunDialog () {
}

}
