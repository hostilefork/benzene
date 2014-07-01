//
// operation.h
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

//
// An Operation represents an action that may mutate the document.
// Besides having their "invoke" method called when the work needs to be
// done, Operation instances are created to represent potential or
// impending work.
//
// Operations typically carry knowledge about the document's state.
// As such, the invocation of any operation will necessarily
// invalidate all others which may be outstanding.  For this reason,
// operations must necessarily be created and managed by the system.
//

#ifndef BENZENE_OPERATION_H
#define BENZENE_OPERATION_H

#include <QtWidgets>

#include "methyl/defs.h"
#include "methyl/node.h"

#include "benzene/application.h"

namespace benzene {


//////////////////////////////////////////////////////////////////////////////
//
// benzene::OperationBase
//
// Operations are templated with the application type they are operations
// on, yet these templates cannot be used internally to the framework.  Hence
// all internal pointers to operations must be kept as benzene::OperationBase.
//

class OperationBase {
public:
    OperationBase () {}
    virtual ~OperationBase() {}

public:
    virtual optional<methyl::RootNode<methyl::Error>> invoke() const = 0;

public:
    methyl::NodeRef<methyl::Node> getDocument() const;

public:
    virtual QString getDescription() const = 0;
};



//////////////////////////////////////////////////////////////////////////////
//
// benzene::Operation<T>
//
// An Operation is an object that represents a desired user action.  Unlike
// the render routines and the rest of the system, inside of the invoke()
// method of an Operation it is legal to modify the document...and then
// return an optional Methyl structure indicating an error condition.
//
// Rendering is expected to take into account the operation and its status,
// whether it is "Glancing", "Hovering, "Pending" or "Running".  These
// Operation status codes are defined in application.h due to technicalities
// of needing to define ApplicationBase fully before defining the operation
// template class.
//

template <class T>
class Operation : public OperationBase
{
    static_assert(
        std::is_base_of<ApplicationBase, T>::value,
        "Operation<> must be parameterized with a Benzene Application class"
    );


protected:
    T & getApplication() const {
        T & result = const_cast<T &>(::benzene::getApplication<T>());
        return result;
    }

    methyl::NodeRef<typename T::NodeType> getDocument() const {
        return *methyl::NodeRef<typename T::NodeType>::checked(
            OperationBase::getDocument()
        );
    }
};

} // end namespace benzene

#endif // BENZENE_OPERATION_H
