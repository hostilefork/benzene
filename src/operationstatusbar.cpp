//
// operationstatusbar.cpp
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

#include "benzene/operationstatusbar.h"

#include "benzene/application.h"

#include "hoist/hoist.h"
using namespace hoist;

namespace benzene {

///////////////////////////////////////////////////////////////////////////////
//
// benzene::OperationStatusBar
//

OperationStatusBar::OperationStatusBar (QWidget * parent) :
    QStatusBar (parent),
    _pixmapError (QPixmap::fromImage(QImage (":/silk/error.png"))),
    _pixmapInformation (QPixmap::fromImage(QImage (":/silk/information.png"))),
    _pixmapMouse (QPixmap::fromImage(QImage (":/silk/mouse.png"))),
    _pixmapHourglass (QPixmap::fromImage(QImage (":/silk/hourglass.png"))),
    _pixmapCursor (QPixmap::fromImage(QImage (":/silk/cursor.png"))),
    _pixmapEye (QPixmap::fromImage(QImage (":/silk/eye.png"))),
    _pixmapExclamation (QPixmap::fromImage(QImage (":/silk/exclamation.png")))
{
    GUI

    _statusBarIcon = new QLabel (this);
    _statusBarMessage = new QLabel (this);

    // OS/X seems to ignore this at the moment
    _statusBarIcon->setFrameShape(QFrame::NoFrame);
    _statusBarIcon->setFrameShadow(QFrame::Plain);
    _statusBarMessage->setFrameShape(QFrame::NoFrame);
    _statusBarMessage->setFrameShadow(QFrame::Plain);

    addWidget(_statusBarIcon);
    addWidget(_statusBarMessage);

    ApplicationBase & app = dynamic_cast<ApplicationBase&>(
        *QApplication::instance()
    );

    app.addStatusBar(*this);
}


void OperationStatusBar::showOperationStatus (
    OperationStatus status,
    QString message
) {
    GUI

    switch (status) {
    case OperationStatus::Glancing:
        _statusBarIcon->setPixmap(_pixmapEye);
        break;
    case OperationStatus::Hovering:
        _statusBarIcon->setPixmap(_pixmapCursor);
        break;
    case OperationStatus::Pending:
        _statusBarIcon->setPixmap(_pixmapMouse);
        break;
    case OperationStatus::Running:
        _statusBarIcon->setPixmap(_pixmapHourglass);
        break;
    default:
        hopefullyNotReached(HERE);
    }
    _statusBarMessage->setText(message);
}


void OperationStatusBar::showInformation (
    QString message
) {
    GUI

    _statusBarIcon->setPixmap(_pixmapInformation);
    _statusBarMessage->setText(message);
}


void OperationStatusBar::showError (
    QString message
) {
    GUI

    _statusBarIcon->setPixmap(_pixmapError);
    _statusBarMessage->setText(message);
}


OperationStatusBar::~OperationStatusBar () {
    GUI

    ApplicationBase & app = dynamic_cast<ApplicationBase&>(
        *QApplication::instance()
    );

    app.removeStatusBar(*this);
}

}
