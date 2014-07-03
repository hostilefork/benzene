//
// hoistdialog.h
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

#ifndef BENZENE_HOISTDIALOG_H
#define BENZENE_HOISTDIALOG_H

#include <QDialog>
#include "hoist/hoist.h"

using namespace hoist;

class QCheckBox;
class QDialogButtonBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QProgressBar;

namespace benzene {

// can only be activated by a mouse click, or keyboard accelerator...(future?)

class HoistDialog : public QDialog
{
	 Q_OBJECT

public:
    HoistDialog (
        codeplace const & cp,
        QString const & message,
        QString const & title
    );

    codeplace getCodeplace() const {
		return _cp;
	}

signals:
	void ignoredOnce();
	void ignoredAll();
	void restarted();
	void debugged();

private:
    codeplace _cp;
	QString _message;
	QString _title;

    QLabel * _messageLabel;
    QLabel * _cpLabel;
    QDialogButtonBox * _buttonBox;

    QPushButton * _ignoreOnceButton;
    QPushButton * _ignoreAllButton;
    QPushButton * _restartButton;
    QPushButton * _debugButton;
};

// It can be useful to add a codeplace to the ignore set...
void ignoreHope(codeplace const & cp);

void onHopeFailed (QString const & message, codeplace const & cp);

} // end namespace benzene

 #endif // BENZENE_HOISTDIALOG_H
