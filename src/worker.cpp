//
// worker.cpp
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

#include "worker.h"
#include "benzene/application.h"

using methyl::NodePrivate;
using methyl::RootNode;
using methyl::Node;
using methyl::Tag;
using methyl::Observer;
using methyl::Context;


namespace benzene {


// For the moment, there is no hook for client code to set the tag of the
// root node of the document; its tag is this one.

methyl::Tag const globalRootOfDocumentTag (HERE);


//////////////////////////////////////////////////////////////////////////////
//
// benzene::WorkerThread
//

WorkerThread::WorkerThread () :
    QThread ()
{
    GUI
}


void WorkerThread::initializeRequest () {
    GUI

    {
        QMutexLocker lock (&_readyMutex);
        start();
        _readyCondition.wait(&_readyMutex);
    }
}


void WorkerThread::run () {
    {
        QMutexLocker lock (&_readyMutex);
        _readyCondition.wakeAll();
    }

    _worker = make_unique<Worker>(*this);

    emit initializeComplete();

    // run the thread's message pump to service Benzene requests
    // while they are still being made
    int result = exec();

    emit exitLoopComplete();

    // From here forward, we should only service the shutdown event.
    // (How to enforce that?)

    int shutdownResult = exec();

    _worker.reset();

    emit shutdownComplete();
}


void WorkerThread::exitLoopRequest () {
    GUI

    exit();
}


void WorkerThread::shutdownRequest () {
    GUI

    exit();
}


WorkerThread::~WorkerThread () {
    GUI

    wait(); // blocks until run() is finished
}



///////////////////////////////////////////////////////////////////////////////
//
// benzene::Worker
//

Worker::Worker (WorkerThread & workerThread) :
    _workerThread (workerThread),
    _daemonManagerThread (new DaemonManagerThread ()),
    _mainWidget (nullptr),
    _status (OperationStatus::None, HERE)
{
    WORKER

    // This artificial delay helps test the automatic progress display if the
    // initialization takes longer than 1 second.

#define ARTIFICIAL_DELAY_FOR_INITIALIZATION 1
#if ARTIFICIAL_DELAY_FOR_INITIALIZATION
    QThread::sleep(3);
#endif

    Worker & worker = *this;

    // unique_ptr?  Sort this interface out...
    methyl::globalEngine = new methyl::Engine(
        [&]() {return worker.contextForCreate();},
        [&]() {return worker.observerInEffect();}
    );

    _root = RootNode<Node>::create(globalRootOfDocumentTag);

    _dummyContext = make_shared<Context>(HERE);

    _daemonManagerThread->initialize();

    connect(
        &_daemonManagerThread->getManager(), &DaemonManager::anyDaemonWritten,
        this, &Worker::onDaemonProgress,
        Qt::QueuedConnection
    );

    connect(
        this, &Worker::queueInvokeOperationMaybe,
        this, &Worker::onQueueInvokeOperationMaybe,
        Qt::QueuedConnection
    );
}


shared_ptr<Context> Worker::contextForCreate () {
    // For now, use a dummy context for all creates.  Longer term we'd make
    // a class derived from Context, that would be able to expire the
    // handles; to implement phasing so that temporary nodes which do not
    // wind up plugged into the document don't accumulate and get stored
    // away somewhere by the rendering code.
    return _dummyContext;
}


shared_ptr<Observer> Worker::observerInEffect () {
    if (isGuiThreadCurrent() || isWorkerThreadCurrent())
        return nullptr;

    QReadLocker lock (&_observersLock);
    auto result = _threadsToObservers.value(QThread::currentThread());
    hopefully(result != nullptr, HERE);
    return result;
}


void Worker::notifyAllBenzenes () {
    WORKER

    auto app = dynamic_cast<ApplicationBase *>(QApplication::instance());

    emit app->benzeneEvent(
        _operation ? (*_operation).get() : nullptr,
        _status
    );
}


void Worker::updateNoLaterThan (unsigned int milliseconds)
{
    WORKER

    QElapsedTimer timer;
    timer.start();
    qint64 desiredUpdate = timer.msecsSinceReference() + milliseconds;

    if (_nextUpdateTick) {
        if (desiredUpdate < *_nextUpdateTick) {
            // We want to update sooner than the last request wanted.
            killTimer(*_nextUpdateTimerId);
            _nextUpdateTimerId = nullopt;
        }
        else {
            // Existing timer will take care of it.
            return;
        }
    }

    // What should be the threshold to just go ahead and update instead of
    // going through the overhead of setting a timer?

    if (milliseconds < 10) {
        notifyAllBenzenes();
        _nextUpdateTick = nullopt;
        return;
    }

    _nextUpdateTick = desiredUpdate;
    _nextUpdateTimerId = startTimer(milliseconds);
}


void Worker::timerEvent (QTimerEvent * event) {
    WORKER

    if (_hoverTimerId and (event->timerId() == *_hoverTimerId)) {
        killTimer(*_hoverTimerId);
        _hoverTimerId = nullopt;

        _status.hopefullyTransition(
            OperationStatus::Glancing,
            OperationStatus::Hovering,
            HERE
        );

        hopefully(_operation != nullopt, HERE);

        emit hoveringOperation((*_operation)->getDescription());

        updateNoLaterThan(30);
    }
    else if (
        _nextUpdateTimerId and (event->timerId() == *_nextUpdateTimerId)
    ) {
        killTimer(*_nextUpdateTimerId);
        _nextUpdateTimerId = nullopt;
        _nextUpdateTick = nullopt;

        notifyAllBenzenes();
    }
    else {
        hopefullyNotReached(HERE);
    }
}


void Worker::receiveGlanceHit (
    methyl::NodePrivate * ownedHit,
    shared_ptr<methyl::Context> const & context
) {
    WORKER

    if (_hoverTimerId) {
        killTimer(*_hoverTimerId);
        _hoverTimerId = nullopt;
    }

    _status.hopefullyInSet(
        OperationStatus::None,
        OperationStatus::Glancing,
        OperationStatus::Hovering,
        HERE
    );

    optional<methyl::RootNode<Hit>> hit
        = methyl::globalEngine->reconstituteRootNode<Hit>(ownedHit, context);

    _hitList.clear();
    _ownedHits.clear();

    if (hit) {
        _hitList.push_back((*hit).get());
        _ownedHits.insert(std::move(*hit));

        _status.assign(OperationStatus::Glancing, HERE);

        _hoverTimerId = startTimer(1000);
    }
    else {
        _status.assign(OperationStatus::None, HERE);
    }

    syncOperation();

    if (_operation and (_status == OperationStatus::Glancing)) {
        emit glancingOperation((*_operation)->getDescription());
    } else {
        emit nullOperation();
    }

    updateNoLaterThan(msecPerceivable);
}


void Worker::receiveFirstHit (
    methyl::NodePrivate * ownedHit,
    shared_ptr<methyl::Context> const & context
) {
    WORKER

    if (_hoverTimerId) {
        killTimer(*_hoverTimerId);
        _hoverTimerId = nullopt;
    }

    optional<methyl::RootNode<Hit>> hit
        = methyl::globalEngine->reconstituteRootNode<Hit>(ownedHit, context);

    _hitList.clear();
    _ownedHits.clear();

    if (hit) {
        _hitList.push_back((*hit).get());
        _ownedHits.insert(std::move(*hit));
    }
    else {
        // What we really want to do if you mouse down on a nullopt
        // Hit is to turn the cursor into the "NO" sign and not let go
        // of the sign until you release the mouse button.  Had to
        // back off the drag and drop API so for now we'll just
        // discard further hits if the list is empty.
    }

    _status.assign(OperationStatus::Pending, HERE);

    syncOperation();

    if (_operation) {
        emit pendingOperation((*_operation)->getDescription());
    } else {
        emit nullOperation();
    }

    updateNoLaterThan(msecPerceivable);
}


void Worker::addHitHelper(optional<methyl::RootNode<Hit>> hit) {
    if (hit) {
        if (not (*_hitList.back())->sameStructureAs((*hit).get())) {
            _hitList.push_back((*hit).get());
            _ownedHits.insert(std::move(*hit));
        } else {
            // Do not push two hits in a row if they are identical
        }
    } else {
        if (not _hitList.back()) {
            // Do nothing if the last element in the list is a nullopt.
            // They indicate discontinuity, so it's wasteful to have
            // multiple nullopts to check for in a row)
        } else {
            // Previous wasn't a nullopt, so push one
            _hitList.push_back(nullopt);
        }
    }
}


void Worker::receiveNextHit (
    methyl::NodePrivate * ownedHit,
    shared_ptr<methyl::Context> const & context
) {
    WORKER

    hopefully(_hoverTimerId == nullopt, HERE);

    _status.hopefullyEqualTo(OperationStatus::Pending, HERE);

    if (_hitList.empty()) {
        // See comments in receiveFirstHit about why we ignore all
        // hits after a nullopt, and why it should be done better
        // with the drag and drop UI.
        return;
    }

    addHitHelper(
        methyl::globalEngine->reconstituteRootNode<Hit>(ownedHit, context)
    );

    syncOperation();

    if (_operation) {
        emit pendingOperation((*_operation)->getDescription());
    } else {
        emit nullOperation();
    }

    updateNoLaterThan(msecPerceivable);
}


void Worker::receiveLastHit (
    methyl::NodePrivate * ownedHit,
    shared_ptr<methyl::Context> const & context
) {
    WORKER

    hopefully(_hoverTimerId == nullopt, HERE);

    _status.hopefullyEqualTo(OperationStatus::Pending, HERE);

    if (_hitList.empty()) {
        // See comments in receiveFirstHit about why we ignore all
        // hits after a nullopt, and why it should be done better
        // with the drag and drop UI.

        _status.assign(OperationStatus::None, HERE);
        return;
    }

    addHitHelper(
        methyl::globalEngine->reconstituteRootNode<Hit>(ownedHit, context)
    );

    syncOperation();

    _status.assign(OperationStatus::Running, HERE);

    // We need to do one last render of the pending operation before
    // we kick off the invocation.  (There will be no way to update the
    // display while the operation is in progress.)  We are about to
    // block the thread so we must call it synchronously.

    notifyAllBenzenes();

    if (_operation) {
        invokeOperation(std::move(*_operation));
    }

    _status.assign(OperationStatus::None, HERE);

    // REVIEW: When an operation is over, we need a way to force a new
    // mouse move in case the user released the mouse button and didn't
    // move again; otherwise they won't get a new glance/hover without
    // touching the mouse

    // When the operation is complete, we need to draw again.
    updateNoLaterThan(msecPerceivable);
}


void Worker::invokeOperation (unique_ptr<OperationBase> operation) {
    WORKER

    emit beginInvokeOperation(operation->getDescription());

    _daemonManagerThread->getManager().ensureAllDaemonsPaused(HERE);

    optional<RootNode<methyl::Error>> result = operation->invoke();

    _daemonManagerThread->getManager().ensureValidDaemonsResumed(HERE);

    if (result) {
        emit endInvokeOperation(false, (*result)->getDescription());
    } else {
        emit endInvokeOperation(
            true,
            operation->getDescription() + " completed successfully."
        );
    }

    updateNoLaterThan(msecPerceivable);
}


void Worker::syncOperation () {
    WORKER

    optional<unique_ptr<OperationBase>> newOperation;

    auto & app = getApplication<ApplicationBase>();

    // First offer we make in the pecking order is a "stroke", for any
    // number of hits.

    newOperation = app.operationForStroke(_hitList);

    if (newOperation) {
        _operation = std::move(*newOperation);
        return;
    }

    // If a series of hits starts and ends on the same hit, we offer
    // the opportunity to think of it as a "Repress".  But if it
    // starts and ends on different hits, it is offered as a "Line"

    if ((_hitList.size() >= 2) and _hitList[0] and _hitList.back()) {
        if ((*_hitList[0])->sameStructureAs(*_hitList.back())) {
            newOperation = app.operationForRepress(*_hitList[0]);
        }
        else {
            newOperation = app.operationForLine(
                *_hitList[0], *_hitList.back()
            );
        }
    }

    if (newOperation) {
        _operation = std::move(*newOperation);
        return;
    }

    // A single element in the hit list is only offered as a "Press"

    if ((_hitList.size() == 1) and _hitList[0]) {
        newOperation = app.operationForPress(*_hitList[0]);
    }

    if (newOperation) {
        _operation = std::move(*newOperation);
        return;
    }

    _operation = nullopt;
}


void Worker::onDaemonProgress () {
    WORKER

    // While we try and keep the reaction time to *explicit user motion*
    // in range of 30fps...when an update comes from background calculation
    // we don't want to burn too many CPU cycles doing updates on every
    // one of them.  So we do 1/3 a second instead of 1/33.

    updateNoLaterThan(msecPerceivable * 10);
}


Worker::~Worker () {
    WORKER

    _daemonManagerThread->shutdown();
    _daemonManagerThread.reset();

    _root = nullopt;

    delete methyl::globalEngine;
    methyl::globalEngine = nullptr;

    // This artificial delay helps test the timer-based progress dialog
    // display if shutdown takes longer than 1 second

#define ARTIFICIAL_DELAY_FOR_SHUTDOWN 1
#if ARTIFICIAL_DELAY_FOR_SHUTDOWN
    QThread::sleep(3);
#endif
}



//////////////////////////////////////////////////////////////////////////////
//
// Thread checking helpers
//

bool isWorkerThreadCurrent () {
    auto & app = getApplication<ApplicationBase>();

    return QThread::currentThread() == &app.getWorkerThread();
}


bool hopefullyWorkerThreadCurrent (codeplace const & cp) {
    return hopefully(isWorkerThreadCurrent(), cp);
}


bool isGuiThreadCurrent () {
    return QThread::currentThread() == QApplication::instance()->thread();
}


bool hopefullyGuiThreadCurrent (codeplace const & cp) {
    return hopefully(isGuiThreadCurrent(), cp);
}

} // end namespace benzene
