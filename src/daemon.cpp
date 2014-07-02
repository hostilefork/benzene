//
// daemon.cpp
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

#include "benzene/daemon.h"

#include "worker.h"

namespace benzene {

///////////////////////////////////////////////////////////////////////////////
//
// benzene::DaemonBase
//

DaemonBase::DaemonBase () :
    _observer (),
    _needsRequeue (false)
{
}


DaemonManager & DaemonBase::getDaemonManager () {
    auto & worker = getApplication<ApplicationBase>().getWorker();

    return worker._daemonManagerThread->getManager();
}


ThinkerManager & DaemonBase::getThinkerManager () {
    return getDaemonManager();
}


optional<ThinkerPresentBase> DaemonBase::tryGetDaemonPresentPrivate (
    methyl::Tree<Descriptor> && descriptor,
    DaemonFactory factory,
    std::type_info const & info
) {
    // requests can come from ENGINE, GUI, or DAEMON
    hopefully(not isDaemonManagerThreadCurrent(), HERE);

    return getDaemonManager().tryGetDaemonPresent(
        std::move(descriptor), factory, info
    );
}


void DaemonBase::afterThreadAttach (ThinkerBase & thinker) {
    getDaemonManager().afterThreadAttach(thinker, *this);
}


void DaemonBase::beforeThreadDetach (ThinkerBase & thinker) {
    getDaemonManager().beforeThreadDetach(thinker, *this);
}


DaemonBase::~DaemonBase () {
}


} // end namespace benzene
