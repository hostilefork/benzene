//
// operationstatusbar.h
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

#ifndef BENZENE_OPERATIONSTATUSBAR_H
#define BENZENE_OPERATIONSTATUSBAR_H

#include <QtWidgets>

#include "pixelcad.h"

namespace benzene {

class ApplicationBase;

///////////////////////////////////////////////////////////////////////////////
//
// benzene::OperationStatusBar
//
// At one point Benzene's desire to "take control" of the application
// structure suggested that it would have to manage the window layout as
// well.  One aspect of this was that the informative status bar needed
// to be managed by the framework.  Compatibility with Qt Creator's UI
// design tool is achieved by simply creating a widget for the Benzene
// status bar, and letting people place it on whatever window makes sense.
//
// Note: To get the widget to work with the "Promote..." function of
// Qt Designer, we have to inherit publicly.  However, this leaves too many
// functions open we don't want clients to call at the moment?  Override with
// private members for things like "clearMessage()" etc?  Or take a future
// direction for an API which can cooperate with user status messages?
//

class OperationStatusBar : public QStatusBar {

    Q_OBJECT

public:
    OperationStatusBar (QWidget * parent = 0);

    ~OperationStatusBar () override;


friend class ApplicationBase;
private slots:
    void showOperationStatus (OperationStatus status, QString message);

    void showInformation (QString message);

    void showError (QString message);


private:
    QLabel * _statusBarIcon;

    QLabel * _statusBarMessage;


private:
    // Loading Pixmaps from the resource file takes time, so we only want to
    // pay that cost for the common pixmaps used once.

    QPixmap _pixmapError;

    QPixmap _pixmapInformation;

    QPixmap _pixmapMouse;

    QPixmap _pixmapHourglass;

    QPixmap _pixmapCursor;

    QPixmap _pixmapEye;

    QPixmap _pixmapExclamation;
};

} // end namespace benzene

#endif // BENZENE_OPERATIONSTATUSBAR_H
