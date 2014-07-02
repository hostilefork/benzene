//
// worker.h
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

#ifndef BENZENE_WORKER_H
#define BENZENE_WORKER_H

#include <unordered_set>

#include <QThread>
#include <QWaitCondition>
#include <QMutex>

#include "methyl/engine.h"

#include "benzene/daemon.h"
#include "benzene/operation.h"
#include "benzene/application.h"

#include "daemonmanager.h"

namespace benzene {

extern methyl::Tag const globalRootOfDocumentTag;

class Worker;


///////////////////////////////////////////////////////////////////////////////
//
// benzene::WorkerThread
//
// We want to run the benzene::Worker on its own thread, and in order to do
// so we need to spawn a thread object.  The thread object's constructor runs
// on the calling thread, so we have to create the Worker during the run()
// method.
//
// The Worker may take an arbitrary amount of time to initialize.  So in order
// for the benzene::Application to keep the GUI thread unblocked and present a
// timer-based progress dialog during initialization and shutdown, there are
// queued signals and slots for the initialization and shutdown requests.
//
// There is a separate request for asking any extant Worker messages that are
// running or have been queued to be finished.  This is helpful in shutdown
// where the Application wants all framework client code to be done running,
// but is not yet ready to invoke the destructor and free thes state that is
// held by the Worker object.
//

class WorkerThread : public QThread
{
    Q_OBJECT

public:
    WorkerThread ();

    ~WorkerThread () override;


protected:
    void run ();


public slots:
    void initializeRequest ();

    void exitLoopRequest();

    void shutdownRequest ();

signals:
    void initializeComplete ();

    void exitLoopComplete();

    void shutdownComplete ();


private:
    unique_ptr<Worker> _worker;

public:
    Worker & getWorker () {
        return *_worker;
    }


private:
    QWaitCondition _readyCondition;

    QMutex _readyMutex;
};


// We offer the client the ability to verify the current thread is GUI, WORKER,
// DAEMON, etc.  But we don't give them a conditional check where it might
// lead to conditional behavior based on which thread is running.  That's why
// the isGuiThreadCurrent() test is in a framework-private header file.

bool isGuiThreadCurrent ();

bool isWorkerThreadCurrent ();



///////////////////////////////////////////////////////////////////////////////
//
// benzene::Worker
//
// The Worker object runs on the benzene::WorkerThread.  It is responsible for
// most of the coordination work of the interaction with client code of the
// framework.  As with most "GUI/Worker" thread separations, the goal is to
// keep the user interface responsive to mouse and painting events.
//
// (There is of course a limit to what the UI can actually be expected to do
// while the Worker is busy, as it holds control of most of the application
// state.  Yet one important thing is to be able to cancel a running
// operation and recover the application state...a friendlier version of
// "Force Quit" followed by recovery.)
//

class Worker : public QObject
{
    Q_OBJECT

public:
    Worker (WorkerThread & workerThread);

    ~Worker () override;


friend class OperationBase;
friend class ApplicationBase;
private:
    // Currently there is only one user document open at a time, and this is
    // the root node of that document.  It could be possible to generalize a
    // "tabbing" interface if some window were designated as a primary
    // document window (the way PhotoShop does with the main work area, which
    // it has tabs on although it reuses the various tool windows).  But for
    // now, multiple documents require multiple application instances.

    optional<methyl::Tree<methyl::Accessor>> _root;

    shared_ptr<methyl::Context> _dummyContext;


friend class DaemonBase;
friend bool ::benzene::isDaemonManagerThreadCurrent();
private:
    WorkerThread & _workerThread;

    unique_ptr<DaemonManagerThread> _daemonManagerThread;


friend class DaemonManager;
private:
    // Methyl provides a hook for a function so that you can tell which
    // observer is currently in effect... so if a read operation happens
    // on a Accessorthen that Observer will get the observation.  The context
    // for which observer we want to use actually comes from which thread
    // is running; so we keep a hash table mapping threads to observers

    QReadWriteLock _observersLock;

    QHash<QThread *, shared_ptr<methyl::Observer>> _threadsToObservers;


private:
    QWidget * _mainWidget;

public:
    void setMainWidget(QWidget & widget) {
        _mainWidget = &widget;
    }

    QWidget & getMainWidget() {
        hopefully(_mainWidget != nullptr, HERE);
        return *_mainWidget;
    }


private:
    optional<qint64> _nextUpdateTick;

    optional<int> _nextUpdateTimerId;

private:
    // Give a hard requirement on how long you're willing to wait for
    // an update.  30 frames per second is the "perceivable" frame rate
    // baseline for human perception, and according to Jeff Johnson in
    // "Gui Bloopers 2.0":
    //
    // "0.1 seconds is the limit for perception of cause-and-effect between
    // events. If software waits longer than 0.1 second to show a response
    // to your action, cause-and-effect is broken; the software's reaction
    // will not seem to be a result of your action (...) If an object the user
    // is 'dragging' lags more than 0.1 second behind the cursor, users will
    // have trouble placing it."
    //
    // http://www.gui-bloopers.com/

    static int const msecPerceivable = 33;

    static int const msecCauseEffect = 100;

    void updateNoLaterThan (unsigned int milliseconds);

    // The timer takes care of redraws as well as hovers.

    void timerEvent (QTimerEvent * event) override;


private:    
    // The Worker is responsible for holding the sequence of benzene::Hit
    // objects representing a gesture made by the user, as well as manage
    // the benzene::Operation objects they represent.  
    //
    // When a benzene::Hit is generated by client code, it is returned as
    // a move-only Tree.  When we pass them back into client code, we
    // send non-owned NodeRefs.  The ordered sequence is stored in _hitList
    // while they are kept from being freed by _ownedHits.

    std::unordered_set<methyl::Tree<Hit>> _ownedHits;

    std::vector<optional<methyl::Node<Hit const>>> _hitList;

    // There is a pecking order in which the references in _hitList are
    // translated into gestures, and offered to client code in order to
    // produce a potential operation.  The _status is determined by issues
    // as to whether the mouse button has been pressed or released, or if
    // a hover timer period has elapsed.

    optional<unique_ptr<OperationBase>> _operation;

    optional<int> _hoverTimerId;

    tracked<OperationStatus> _status;

public slots:
    // Though a Hit may originate from multiple threads, they are queued and
    // always managed on the WorkerThread.  Hence no locking is needed.

    void receiveGlanceHit (
        methyl::NodePrivate * ownedHit,
        shared_ptr<methyl::Context> const & context
    );

    void receiveFirstHit (
        methyl::NodePrivate * ownedHit,
        shared_ptr<methyl::Context> const & context
    );

    void receiveNextHit (
        methyl::NodePrivate * ownedHit,
        shared_ptr<methyl::Context> const & context
    );

    void receiveLastHit (
        methyl::NodePrivate * ownedHit,
        shared_ptr<methyl::Context> const & context
    );

private:
    void addHitHelper (optional<methyl::Tree<Hit>> hit);

    void syncOperation();


// Operations are always invoked on the worker, so that the GUI thread can
// offer a progress dialog if a certain amount of time is taken.  This is
// the only time that a client object can have write access on user documents.

signals:
    void beginInvokeOperation (QString message);

    void endInvokeOperation (bool success, QString message);

    void glancingOperation (QString message);

    void hoveringOperation (QString message);

    void pendingOperation (QString message);

    void nullOperation ();

public:

    void invokeOperation (unique_ptr<OperationBase> operation);

signals:
    // Here we have a conundrum.  What do we do if an operation is
    // queued and we have already applied another operation which
    // might invalidate the expectations of the other operation?
    //
    // There's no easy solution I can think of besides possibly just
    // throwing this operation out.  For now we'll just risk it.

    void queueInvokeOperationMaybe (OperationBase * opOwned);

private slots:
    void onQueueInvokeOperationMaybe (OperationBase * opOwned) {
        WORKER

        invokeOperation(unique_ptr<OperationBase>(opOwned));
    }


private:
    void notifyAllBenzenes ();

public slots:
    void onDaemonProgress ();

private:
    shared_ptr<methyl::Observer> observerInEffect();

    shared_ptr<methyl::Context> contextForCreate();
};


} // end namespace benzene

#endif // BENZENE_WORKER_H
