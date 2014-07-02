//
// daemonmanager.cpp
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
#include "benzene/daemon.h"

#include "thinkerqt/thinkerrunner.h"

using methyl::Tree;
using methyl::Node;
using methyl::NodePrivate;
using methyl::Context;

namespace benzene {


///////////////////////////////////////////////////////////////////////////////
//
// benzene::DaemonManagerThread
//

DaemonManagerThread::DaemonManagerThread () :
    QThread ()
{
    WORKER
}


void DaemonManagerThread::initialize () {
    WORKER

    {
    QMutexLocker lock (&_readyMutex);
    start();
    _readyCondition.wait(&_readyMutex);
    }
}


void DaemonManagerThread::run () {
    // We want the Daemon Manager to have thread affinity on the Daemon
    // Manager thread.  Can't do thread checks until the worker is finished
    // initializing...which only happens when initialize is finished.

    _daemonManager = make_unique<DaemonManager>();

    {
    QMutexLocker lock (&_readyMutex);
    _readyCondition.wakeAll();
    }

    exec();

    _daemonManager.reset();
}


void DaemonManagerThread::shutdown () {
    WORKER

    quit();

    // blocks until run() is finished
    wait();
}


DaemonManagerThread::~DaemonManagerThread () {
    WORKER
}



///////////////////////////////////////////////////////////////////////////////
//
// benzene::DaemonManager
//

DaemonManager::DaemonManager () {
    qRegisterMetaType<DaemonFactory>("DaemonFactory");

    connect(
        this, &ThinkerManager::anyThinkerWritten,
        this, &DaemonManager::anyDaemonWritten,
        Qt::DirectConnection
    );

    connect(
        this, &DaemonManager::daemonCreateRequest,
        this, &DaemonManager::onDaemonCreateRequest,
        Qt::QueuedConnection
    );

    connect(
        this, &DaemonManager::ensureDaemonsPausedBlocking,
        this, &DaemonManager::onEnsureDaemonsPausedBlocking,
        Qt::BlockingQueuedConnection
    );

    connect(
        this, &DaemonManager::ensureDaemonsResumedBlocking,
        this, &DaemonManager::onEnsureDaemonsResumedBlocking,
        Qt::BlockingQueuedConnection
    );
}



optional<ThinkerPresentBase> DaemonManager::createOrRequeueDaemon (
    methyl::Tree<Descriptor> descriptor,
    DaemonFactory factory,
    std::type_info const & info,
    qint64 requestTick
) {
    // In the current architectural state, you can snapshot a Daemon from
    // pretty much any thread... including one Daemon snapshotting another.
    // The only thread we can really rule out is the Daemon Manager, which
    // will be servicing the creation of the Daemon if that is what's needed
    hopefully(not isDaemonManagerThreadCurrent(), HERE);

    unique_ptr<NodePrivate> nodePrivateOwned;
    shared_ptr<Context> context;

    std::tie(nodePrivateOwned, context)
        = methyl::globalEngine->dissectTree(std::move(descriptor));

    // We have to pass the factory by value, but the lifetime of the
    // typeinfo is until end of program:
    //
    //     http://stackoverflow.com/questions/7024818/
    emit daemonCreateRequest(
        nodePrivateOwned.release(), context, factory, &info, requestTick
    );

    return nullopt;
}


void DaemonManager::afterThreadAttach (
    ThinkerBase & thinker,
    DaemonBase & daemon
) {
    DAEMON

    auto & worker = getApplication<ApplicationBase>().getWorker();

    QWriteLocker lock (&worker._observersLock);
    worker._threadsToObservers[thinker.thread()] = daemon._observer;
}


void DaemonManager::beforeThreadDetach (
    ThinkerBase & thinker,
    DaemonBase & daemon
) {
    DAEMON

    auto & worker = getApplication<ApplicationBase>().getWorker();

    QWriteLocker lock (&worker._observersLock);
    worker._threadsToObservers.remove(thinker.thread());
}


optional<ThinkerPresentBase> DaemonManager::tryGetDaemonPresent (
    methyl::Tree<Descriptor> descriptor,
    DaemonFactory factory,
    std::type_info const & info
) {
    // In the current architectural state, you can snapshot a Daemon from
    // pretty much any thread... including one Daemon snapshotting another.
    // The only thread we can really rule out is the Daemon Manager, which
    // will be servicing the creation of the Daemon if that is what's needed
    hopefully(not isDaemonManagerThreadCurrent(), HERE);

    QElapsedTimer timer;
    timer.start();

    using methyl::Accessor;
    using methyl::Node;

    {
        QReadLocker lock (&_daemonMapLock);
        auto itType = _daemonMap.find(&info);
        if (itType != _daemonMap.end()) {
            auto it = itType->second.find(descriptor.get());
            if (it != itType->second.end()) {
                auto & daemon = dynamic_cast<DaemonBase &>(
                    getThinkerBase(it->second)
                );
                daemon._lastRequestTick = timer.msecsSinceReference();
                if (not daemon._needsRequeue)
                    return it->second;
            }
        }
    }

    return createOrRequeueDaemon(
        std::move(descriptor), factory, info, timer.msecsSinceReference()
    );
}


void DaemonManager::onDaemonCreateRequest (
    methyl::NodePrivate * descriptorOwned,
    shared_ptr<methyl::Context> contextOwned,
    DaemonFactory factory,
    std::type_info const * info,
    qint64 requestTick
) {
    DAEMONMANAGER

    optional<Tree<Descriptor>> newDescriptor
        = methyl::globalEngine->reconstituteTree<Descriptor>(
            descriptorOwned, contextOwned
        );

    hopefully(newDescriptor != nullopt, HERE);

    auto createPresent = [&](Tree<Descriptor> descriptor)
        -> ThinkerPresentBase
    {
        QElapsedTimer timer;
        timer.start();

        // The descriptor was created with a different observation context
        // than we want the Daemon's observations to be using.  So all the
        // observations need to be made through a new monitoring facility
        // based on the observer in this Daemon.

        auto context = make_shared<Context>(HERE);
        Node<Descriptor const> descriptorRef
            = methyl::globalEngine->contextualNodeRef(
                descriptor.get(), context
            );

        // This is tricky, because we are being asked to tell the observer
        // all the roots of trees we want to watch and become invalid if
        // they are modified.  Technically speaking we'd care if the
        // descriptor were to change, but it isn't allowed to so we
        // don't have to mention it.  A Daemon is free to create and
        // manipulate new nodes, and we wouldn't want creating a node and
        // observing it then destroying it to count as an invalidation
        // (the Daemon knew what it was doing, presumably...and this is
        // not an invalidation of the input to the process...it's no
        // different than using any other temporary variables)
        //
        // So for now we only consider the root of the document (and in
        // the future this would be the root of all user documents; and
        // basically any other state which might be considered a relevant
        // input).  This is still formative, but seems to work for now.

        auto & app = getApplication<ApplicationBase>();
        auto & worker = app.getWorker();

        auto observer = methyl::Observer::create(app.getDocument(), HERE);

        {
            // To keep from blocking the worker, we call the Daemon
            // constructor from the DaemonManagerThread.  We need to
            // make sure that Methyl knows to log observations of
            // data to the observer we're going to put into the Daemon
            // after creation.

            QWriteLocker lock (&worker._observersLock);
            worker._threadsToObservers[QThread::currentThread()] = observer;
        }

        unique_ptr<ThinkerBase> thinker = factory(descriptorRef);

        {
            // Further running of the Daemon will be from the thread
            // pool, so remove the association of node observations
            // from the DaemonManager thread.

            QWriteLocker lock (&worker._observersLock);
            worker._threadsToObservers.remove(QThread::currentThread());
        }

        DaemonBase & daemon = dynamic_cast<DaemonBase &>(*thinker);

        // Set the internal properties.  These "belong" to the manager,
        // but it's more efficient to poke them into the Daemon itself
        // to avoid the hash table storage and lookup.

        daemon._descriptor = std::move(descriptor);
        daemon._context = context;
        daemon._lastRequestTick = requestTick;
        daemon._msecsUsed = timer.elapsed();
        daemon._observer = observer;

        return runBase(std::move(thinker), HERE);
    };

    QWriteLocker lock (&_daemonMapLock);

    auto itType = _daemonMap.find(info);
    if (itType == _daemonMap.end()) {
        auto newDescriptorRef = (*newDescriptor).get();
        _daemonMap[info].insert(std::make_pair(
            newDescriptorRef, createPresent(std::move(*newDescriptor))
        ));
    } else {
        // It is possible that the same Daemon descriptor will be queued
        // multiple times before the Daemon creation thread gets around
        // to doing the creation.  Rather than try and create a data
        // structure for a queue and avoid inserting duplicates, we
        // let the duplicates pile up in the thread's event queue.  Then
        // if a redundant request comes along we just ignore it.  It's
        // a simple implementation but a little bit "sloppier" than I
        // usually like :-/ but going with it until there's a problem.

        auto itPair = itType->second.find((*newDescriptor).get());
        if (itPair == itType->second.end()) {
            auto newDescriptorRef = (*newDescriptor).get();
            itType->second.insert(std::make_pair(
                newDescriptorRef, createPresent(std::move(*newDescriptor))
            ));
        } else {
            // We'll get here if there was a request for this daemon already
            // serviced -or- if there is a requeue request.

            ThinkerPresentBase present;
            std::tie(std::ignore, present) = *itPair;

            DaemonBase & oldDaemon = dynamic_cast<DaemonBase &>(
                this->getThinkerBase(present)
            );

            if (oldDaemon._needsRequeue) {

                oldDaemon._needsRequeue = false;

                // In theory we should be able to just set the _needsRequeue
                // flag here and run the Daemon from continue().  But at the
                // moment, the run() methods really expect a Thinker to
                // come in as a unique_ptr; so we sort of have to delete
                // the oldDaemon and make a new one from scratch.  The
                // requeue is a stopgap measure anyway, so REVIEW.

                // We can't overwrite the key in place, only the value.
                // Because the key is a NodeRef to the old descriptor,
                // we have to use the old Tree in order to keep
                // that NodeRef good.  Requeuing "starts the Daemon from
                // the top" which means the whole construction happens
                // again, not just continue()ing or start()ing.  It all
                // needs to be rethought, but basically all the Daemon
                // state gets thrown away here and a new Daemon is
                // created - that's excessive.

                // http://www.cplusplus.com/forum/general/26950/
                itPair->second = createPresent(
                    std::move(*oldDaemon._descriptor)
                );
            } else {
                // a request for this descriptor already serviced; ignore.
            }
        }
    }
}


void DaemonManager::onEnsureDaemonsPausedBlocking (codeplace cp) {
    DAEMONMANAGER

    ThinkerManager::ensureThinkersPaused(cp);
}


void DaemonManager::onEnsureDaemonsResumedBlocking (codeplace cp) {
    DAEMONMANAGER

    // Look for any daemons that are now invalid; they have to be
    // destroyed
    for (auto & typeinfoAndMap : _daemonMap) {

        auto & nodeToDaemonPresentMap = typeinfoAndMap.second;

        auto it = begin(nodeToDaemonPresentMap);

        while (it != end(nodeToDaemonPresentMap)) {
            ThinkerPresentBase & daemonPresent = it->second;

            DaemonBase & daemon = dynamic_cast<DaemonBase &>(
                getThinkerBase(daemonPresent)
            );

            if (daemon._observer->isBlinded()) {
                auto itErase (it);
                it++;

                daemonPresent.cancel();

                // free the daemon, the observer upon which it
                // calculated are no longer correct; it will be
                // recreated again if it is needed.
                nodeToDaemonPresentMap.erase(itErase);
            } else {
                it++;
            }
        }
    }

    ThinkerManager::ensureThinkersResumed(HERE);
}


void DaemonManager::ensureAllDaemonsPaused (codeplace const & cp) {
    WORKER

    emit ensureDaemonsPausedBlocking(cp);
}


void DaemonManager::ensureValidDaemonsResumed (codeplace const & cp) {
    WORKER

    emit ensureDaemonsResumedBlocking(cp);
}


DaemonManager::~DaemonManager () {
    // Can't use DAEMONMANAGER here because globalWorker, which holds onto
    // the pointer for the thread, has been nulled during the destructor
    // to avoid partial usages.

    hopefully(QThread::currentThread() == thread(), HERE);

    // ThinkerManager and ThinkerPresents demand destruction on the
    // thread they were started on.  As a Thinker::Present is a shared
    // pointer under the hood this does not guarantee they will all be
    // destructed before the thinkerManager is reset, but from the
    // engine's point of view those are the only handles we have.

    ThinkerManager::ensureThinkersPaused(HERE);
    _daemonMap.clear();
}



///////////////////////////////////////////////////////////////////////////////
//
// Thread checking helpers
//

bool isDaemonManagerThreadCurrent () {
    auto & worker = getApplication<ApplicationBase>().getWorker();

    return worker._daemonManagerThread.get() == QThread::currentThread();
}


bool hopefullyDaemonManagerThreadCurrent (codeplace const & cp) {
    return hopefully(isDaemonManagerThreadCurrent(), cp);
}


bool isDaemonThreadCurrent () {
    // not the best test, there could be other threads we didn't start
    // involved somehow.
    return not isGuiThreadCurrent()
        and not isWorkerThreadCurrent()
        and not isDaemonManagerThreadCurrent();
}


bool hopefullyDaemonThreadCurrent (codeplace const & cp) {
    return hopefully(isDaemonThreadCurrent(), cp);
}

} // end namespace benzene
