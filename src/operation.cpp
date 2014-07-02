//
// operation.cpp
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

#include "benzene/operation.h"

#include "worker.h"

namespace benzene {

///////////////////////////////////////////////////////////////////////////////
//
// benzene::OperationBase
//

methyl::Node<methyl::Accessor> OperationBase::getDocument() const {
    WORKER

    // Only the worker can call this method on Operation for getting write
    // access to the document.  It should additionally be enforced that
    // the access is given only during invoke().
    //
    // REVIEW: Is there a better way to formalize this as a parameter to
    // the invoke method?

    auto & worker = getApplication<ApplicationBase>().getWorker();

    return methyl::globalEngine->contextualNodeRef(
        (*worker._root).get(), worker._dummyContext
    );
}

} // end namespace benzene
