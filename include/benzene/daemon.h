//
// daemon.h
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


#ifndef BENZENE_DAEMON_H
#define BENZENE_DAEMON_H

#include <unordered_set>

#include "methyl/node.h"
#include "methyl/observer.h"
#include "methyl/engine.h"

#include "thinkerqt/snapshottable.h"
#include "thinkerqt/thinker.h"


namespace benzene {

// As with Hit, this is just a typedef.  Whether it is necessary or not I
// don't know, but for now it helps with documentation.

typedef methyl::Node Descriptor;


template <class> class Daemon;

class DaemonManager;


// The Benzene framework is the one doing the creation of Daemons on your
// behalf, via snapshot<DaemonType>(descriptor).  Yet it needs to create the
// underlying Thinker on the ThinkerManager thread regardless of what thread
// makes the request...and it must be able to make an instance of that
// templated type.  A DaemonFactory is what lets us pass the properly
// type templated code to do the allocation to the ThinkerManager thread to
// do the allocation of the derived type from generic code which only knows
// about the DaemonBase.

typedef std::function<
    unique_ptr<ThinkerBase>(methyl::NodeRef<Descriptor const>)
> DaemonFactory;



//////////////////////////////////////////////////////////////////////////////
//
// benzene::DaemonBase
//
// At this point in time, a Daemon is essentially a Thinker from the library
// Thinker-Qt, which is documented here:
//
//     http://thinker-qt.hostilefork.com
//
// Yet benzene::Daemon is a templated Q_OBJECT that wishes to derive directly
// from the templated Thinker<T>.  If the DaemonBase were to try and wedge
// into the inheritance hierarchy, it would have to inherit from ThinkerBase
// and would wind up rewriting the templated layer on top of that.
//
// (The reason for these XXXBase classes is that Q_OBJECTs cannot have signals
// or slots declared inside of them if they are a template.  This is a
// limitation of Qt's Meta-Object compiler (moc)).
//
// For that reason, we use the "simpler" multiple-inheritance solution, and
// the DaemonBase class is *not* a Q_OBJECT.  (You can't multiply inherit
// from Q_OBJECT classes either, even with virtual inheritance.)  It holds
// the Daemon-specific properties of interest to the framework.  That could
// also be achieved with a lookup table, but this is less overhead and
// less leak-prone.
//

class DaemonBase {

public:
    enum class Status {
        Complete,
        Pause,
        Dependent
    };


friend class DaemonManager;
private:
    // We poke the owned descriptor in here behind the curtain so that the
    // derived class doesn't have to pass the owned descriptor through.
    // hack for the moment is to make it optional as you can't default-init
    // a RootNode, but consider extracting the NodePrivate or other trickery

    optional<methyl::RootNode<Descriptor>> _descriptor;

    shared_ptr<methyl::Context> _context;

    shared_ptr<methyl::Observer> _observer;


private:
    // The DaemonManagerThread has a periodic timer task to go through and
    // free Daemons that look like good candidates for garbage collection.
    // A primary concern is when it was most recently requested; but
    // other measures of interest are how long it took to generate vs.
    // how large the DaemonData size are.  Also, if a high-priority
    // Daemon is unfinished and has registered a dependency on the data,
    // we don't want to free it.

    qint64 _lastRequestTick;

    qint64 _msecsUsed;

    std::unordered_set<methyl::NodeRef<Descriptor const>> _dependents;


template<class> friend class Daemon;
private:
    bool _needsRequeue;


friend class DaemonCreateThread;
protected:    
    static ThinkerManager & getThinkerManager ();

    static DaemonManager & getDaemonManager ();


private:
    template <class DaemonType, class... Args> friend
    optional<typename DaemonType::Snapshot> trySnapshotDaemon (
        Args &&... args
    );

    static optional<ThinkerPresentBase> tryGetDaemonPresentPrivate (
        methyl::RootNode<Descriptor> descriptor,
        DaemonFactory factory,
        std::type_info const & info
    );


protected:
    virtual Status startDaemon () = 0;

    virtual Status resumeDaemon () = 0;


protected:
    void afterThreadAttach (ThinkerBase & thinker);

    void beforeThreadDetach (ThinkerBase & thinker);


public:
    DaemonBase ();

    virtual ~DaemonBase();
};


//
// May need to be able to navigate back to the DaemonBase to offer some sort
// of "extra" protocol beyond what SnapshottableData offers.
// 
//     https://github.com/hostilefork/benzene/issues/6
//
// No good use cases yet, but futureproofing in case one comes up.
//

class DaemonData : public SnapshottableData {
};



//////////////////////////////////////////////////////////////////////////////
//
// benzene::Daemon<T>
//
// A Daemon is an entity that is dispatched to do background processing.
// It is identified by a benzene::Descriptor, which is implemented as an
// identity-comparable Methyl structure.  If two descriptors have the same
// structure, then they will identify the same Daemon.
//
// Daemons are not communicated with directly.  Client code uses the function
// benzene::trySnapshotDaemon() to try and get a snapshot of the state of
// the progress of calculation of a particular Daemon.  If that snapshot
// comes back as a nullopt, then the Daemon will have been queued but no
// calculation state is yet available.  If not, then a snapshot object is
// returned which can be safely queried to read the calculation state.
//
// If it happens that the running Daemon needs to update the state that a
// client is observing while it holds a snapshot, then the Daemon will pay
// for a copy on its own thread to do so.  This copy-on-write strategy
// prevents a background calculation from blocking clients, such as a GUI
// render thread.
//
// While a Daemon does not need to be interrupted by read-only operations
// like renders, an operation that mutates the document *may* invalidate an
// observation it made to produce its calculation.  That mutating operation
// may even destroy document nodes that the Daemon is working with.
//
// For this reason, mutating operations happen only after all Daemons have
// been put into a "paused" state.  They are required to clear themselves
// off the stack and return control to the scheduler.  The scheduler may
// return control to them if the operation completes and no effects to
// its previous observer are noted.  But any changes will mean the
// Daemon is destroyed and would have to start from scratch if the
// calculation is requested again.
//
// Some Daemons build their calculations on work done by other Daemons.
// However, these derived Daemons may only run if the other Daemon is
// finished.  If they ask for a snapshot and receive nothing back, then
// that means they need to yield to the scheduler.  They may request
// several different depedencies but will only be called again when
// those dependencies have been satisfied.
//

template <class T>
class Daemon : public Thinker<T>, public DaemonBase
{
    static_assert(
        std::is_base_of<DaemonData, T>::value,
        "Daemon<> must be parameterized with a DaemonData-derived class"
    );


public:
    template <class... Args>
    Daemon (Args &&... args) :
        Thinker<T> (
            // Forward declaration of DaemonManager means we can't get the
            // automatic coercion from DaemonManager => ThinkerManager
            getThinkerManager(),
            std::forward<Args>(args)...
        ),
        _firstRun (true)
    {
    }


private:
    bool _firstRun;

    bool start() override final {
        Status status = _firstRun ? startDaemon() : resumeDaemon();
        _firstRun = false;
        switch (status) {
        case Status::Complete:
            return true;
        case Status::Pause:
            return false;
        case Status::Dependent:
            hopefully(not _needsRequeue, HERE);
            _needsRequeue = true;
            return true;
        }

        throw hopefullyNotReached(HERE);
    }

    bool resume() override final {
        Status status = resumeDaemon();
        switch (status) {
        case Status::Complete:
            return true;
        case Status::Pause:
            return false;
        case Status::Dependent:
            hopefully(not _needsRequeue, HERE);
            _needsRequeue = true;
            return true;
        }

        throw hopefullyNotReached(HERE);
    }


protected:
    void afterThreadAttach () override {
        DaemonBase::afterThreadAttach(*this);
    }

    void beforeThreadDetach () override {
        DaemonBase::beforeThreadDetach(*this);
    }

public:
    ~Daemon() override {}
};



///////////////////////////////////////////////////////////////////////////////
//
// benzene::trySnapshotDaemon()
//
// This global operation attempts to snapshot a Daemon of a particular type.
// If the Daemon does not exist yet, then it wraps up the construction into
// a DaemonFactory lambda function.  (For this reason, it cannot be a
// trySnapshot member of Daemon<T>...because we want to construct a class
// *derived* from Daemon<T>; that's only possible if we are passed the
// identity of that derived class).
// Can't put this in the Daemon template and manage to access the derived type
// for the allocation... has to be a global function or a member of another
// class.  No class in particular makes sense, so why not a global template?
//

template <class T, class... Args>
optional<typename T::Snapshot> trySnapshotDaemon(
    Args &&... args
) {
    static_assert(
        std::is_base_of<DaemonBase, T>::value,
        "trySnapshotDaemon<>() must be parameterized with a Daemon class"
    );

    DaemonFactory factory (
        [] (methyl::NodeRef<Descriptor const> descriptor) {
            return unique_ptr<ThinkerBase> (
                new T (T::unpackDescriptor(descriptor))
            );
        }
    );

    methyl::RootNode<Descriptor> descriptor = T::packDescriptor (
        std::forward<Args>(args)...
    );

    // If this thread is actually a Daemon thread requesting, this call may
    // be blocking - as when one Daemon depends upon another there is no
    // incrementality at this time.  This may be a breaking assumption for
    // the architecture--it would mean for instance that a long palette
    // computation could hold up the entirety of an outline operation
    // that depended on that outline.

    optional<ThinkerPresentBase> presentBase = T::tryGetDaemonPresentPrivate (
        std::move(descriptor), factory, typeid(T)
    );

    // There wasn't a Daemon matching this descriptor available (yet)
    // It should have been put into the creation queue where the factory
    // will be used to make it.  But we don't want to block the calling
    // thread and make it wait for that creation.

    if (not presentBase)
        return nullopt;

    return (typename T::Present (*presentBase)).createSnapshot();
}

} // end namespace benzene

Q_DECLARE_METATYPE(benzene::DaemonFactory)

#endif // BENZENE_DAEMON_H
