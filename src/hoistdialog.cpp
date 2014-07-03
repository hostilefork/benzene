//
// hoistdialog.cpp
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

#include "boost/assert.hpp"

#include "benzene/application.h"

#include "worker.h"
#include "hoistdialog.h"

namespace benzene {

HoistDialog::HoistDialog (
    codeplace const & cp,
    QString const & message,
    QString const & title
) :
    QDialog(nullptr),
	_cp (cp),
	_message (message),
	_title (title)
{
	setWindowTitle(title);

    _messageLabel = new QLabel(message);
    _cpLabel = new QLabel(cp.toString());

    _ignoreOnceButton = new QPushButton("Ignore &Once");
    _ignoreAllButton = new QPushButton("Ignore &All");
    _restartButton = new QPushButton("&Restart");
    _restartButton->setAutoDefault(true);
    _debugButton = new QPushButton("&Debug");

    _buttonBox = new QDialogButtonBox(Qt::Horizontal);
    _buttonBox->addButton(_ignoreOnceButton, QDialogButtonBox::ActionRole);
    _buttonBox->addButton(_ignoreAllButton, QDialogButtonBox::ActionRole);
    _buttonBox->addButton(_restartButton, QDialogButtonBox::ActionRole);
    _buttonBox->addButton(_debugButton, QDialogButtonBox::ActionRole);

    connect(
        _ignoreOnceButton, &QPushButton::clicked,
        this, &HoistDialog::ignoredOnce
    );

    connect(
        _ignoreAllButton, &QPushButton::clicked,
        this, &HoistDialog::ignoredAll
    );

    connect(
        _restartButton, &QPushButton::clicked,
        this, &HoistDialog::restarted
    );

    connect(
        _debugButton, &QPushButton::clicked,
        this, &HoistDialog::debugged
    );

    QVBoxLayout * mainLayout = new QVBoxLayout;
	mainLayout->setSizeConstraint(QLayout::SetFixedSize);
    mainLayout->addWidget(_messageLabel);
    mainLayout->addWidget(_cpLabel);
    mainLayout->addWidget(_buttonBox);
	setLayout(mainLayout);
}



void ignoreHope (codeplace const & cp) {
    auto & app = dynamic_cast<ApplicationBase &>(*QApplication::instance());

    // TODO: LOCK!
    app._hopesToIgnore.insert(cp);

}

void onHopeFailed (QString const & message, codeplace const & cp) {
    tracked<bool> shouldChronicleHopefully (true, HERE);
    chronicle(shouldChronicleHopefully, message, cp);

    auto & app = dynamic_cast<ApplicationBase &>(*QApplication::instance());

    if (app._hopesToIgnore.find(cp) != app._hopesToIgnore.end()) {
        chronicle(shouldChronicleHopefully, "^-- IGNORING HOPE!", HERE);
        return;
    }

    if (isGuiThreadCurrent()) {
        // This is a diret connection; it will block

        app.onHopeFailed(message, cp);
    } else {
        // emit a hope failure to the application on the gui thread
        // It does a cross-thread blocking queued connection

        emit app.hopeFailed(message, cp);
    }
}

} // end namespace benzene



namespace boost {

// These are the two variants of the boost assertion handler that you need
// to override, for BOOST_ASSERT and BOOST_ASSERT_MSG
//
// NOTE: make sure you have compiler defined -DBOOST_ENABLE_ASSERT_HANDLER=1
//
// by using HERE() we will identify this with a guid consistent to this
// location.  That guid however will be associated with several different
// file/line numbers.  This is interesting because it's a non-ephemeral
// codeplace which exists at multiple source locations...a bit like if you
// used the same UUID for HERE on two different source lines.

void assertion_failed (
    char const * expr,
    char const * function,
    char const * file,
    long line
) {
    hopefullyNotReached(
        QString (function) + QString (expr),
        THERE(QString (file), line, HERE)
    );
}


void assertion_failed_msg (
    char const * expr,
    char const * function,
    char const * file,
    char const * message,
    long line
) {
    hopefullyNotReached(
        QString (function) + QString (expr) + QString (message),
        THERE(QString (file), line, HERE)
    );
}


} // end namespace boost
