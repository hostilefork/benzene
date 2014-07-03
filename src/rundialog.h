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

#ifndef BENZENE_RUNDIALOG_H
#define BENZENE_RUNDIALOG_H

#include <QDialog>

class QCheckBox;
class QDialogButtonBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QProgressBar;

#include "methyl/defs.h"

///////////////////////////////////////////////////////////////////////////////
//
// benzene::RunDialog
//
// The framework is not currently set up in such a way to have percentage
// progress notifications... although it could easily show something like the
// elapsed time.
//
// One idea in the original Benzene framework was to facilitate the termination
// of operations that were running too long, and to make restoring from the
// transaction log seem just like any other error from the user interface.
// This has been lost in the cross-platform transition to Qt, as the
// transparent relaunching of a process would involve a process API.
//
	
namespace benzene {

class RunDialog : public QDialog
{
	 Q_OBJECT
	 
public:
    RunDialog (QWidget *parent = 0);

    ~RunDialog () override;

    void setProgressString (QString message);


private:
    void timerEvent (QTimerEvent * event) override;


signals:
    void canceled ();

    void terminated ();


public slots:
    void requestClose ();

signals:
    // Emitted when enough time has elapsed after being shown to not look
    // "flickery" (if it was shown, otherwise this is emitted immediately)
    void okayToClose ();

private:
    QLabel * _label;
    QProgressBar * _progress;
    QDialogButtonBox * _buttonBox;

    QPushButton * _cancelButton;
    QPushButton * _terminateButton;

    optional<qint64> _tickShown;
};

} // end namespace benzene

#endif // BENZENE_RUNDIALOG_H
