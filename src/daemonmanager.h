//
// daemonmanager.h
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

#ifndef BENZENE_DAEMONMANAGER_H
#define BENZENE_DAEMONMANAGER_H

#include <QThread>
#include <QWaitCondition>
#include <QMutex>

#include <map>

#include "benzene/daemon.h"
#include "thinkerqt/thinkermanager.h"

namespace benzene {


class DaemonManager;


class DaemonManagerThread : public QThread
{
    Q_OBJECT

public:
    DaemonManagerThread ();

    ~DaemonManagerThread () override;

public:
    void initialize ();

    void shutdown ();

protected:
    void run();

private:
    unique_ptr<DaemonManager> _daemonManager;

public:
    DaemonManager & getManager () {
        return *_daemonManager;
    }

private:
    QMutex _readyMutex;

    QWaitCondition _readyCondition;
};



class DaemonManager : public ThinkerManager {
    Q_OBJECT

    // What this does and why is described here: 
    //
    //    http://stackoverflow.com/questions/19597838/
    //
    // Basically any pairing of a particular Daemon<T> type and a descriptor
    // that compares equally will yield the same element in the map.

private:
    struct less_dereferenced_type_info {
        // http://stackoverflow.com/questions/3552135/#comment3721776_3552210
        bool operator() (
            std::type_info const * left, std::type_info const * right
        ) const
        {
            return left->before(*right);
        }
    };


    struct hash_dereferenced_type_info {
        // http://stackoverflow.com/questions/3552135/#comment3721776_3552210
        size_t operator() (std::type_info const * const & value) const
        {
            return value->hash_code();
        }
    };


    // Test for actual equality of dereferenced type_info
    struct equal_dereferenced_type_info {
    public:
        size_t operator() (
            std::type_info const * left,
            std::type_info const * right
        ) const
        {
            return *left == *right;
        }
    };

private:
    friend class DaemonManagerThread;

    // You are not guaranteed to get the same pointer back for type_info
    // each time you call.  But we are using a compare function that
    // dereferences the pointer so that two different pointers that
    // indicate the same value will compare equally
    std::unordered_map<
        std::type_info const *,
        std::unordered_map<
            methyl::Node<Descriptor const>,
            ThinkerPresentBase,
            methyl::structure_hash<methyl::Node<Descriptor const>>,
            methyl::structure_equal_to<methyl::Node<Descriptor const>>
        >,
        hash_dereferenced_type_info,
        equal_dereferenced_type_info
    > _daemonMap;

    QReadWriteLock _daemonMapLock;


public:
    DaemonManager ();

private:
    // The process of creating a Daemon takes time to unpack the descriptor
    // and allocating the snapshottable state.  In order to avoid holding up
    // the render or whatever triggered the request for a Daemon that has not
    // yet been started, it is handled by an independent thread.
    optional<ThinkerPresentBase> createOrRequeueDaemon (
        methyl::Tree<Descriptor> descriptor,
        DaemonFactory factory,
        std::type_info const & info,
        qint64 requestTick
    );


signals:
    void daemonCreateRequest (
        methyl::NodePrivate * rootHit,
        shared_ptr<methyl::Context> obs,
        DaemonFactory factory,
        std::type_info const * info,
        qint64 requestTick
    );

public slots:
    // Pointer args are never nullptr, but no refs allowed in slots
    void onDaemonCreateRequest (
        methyl::NodePrivate * descriptorOwned,
        shared_ptr<methyl::Context> context,
        DaemonFactory factory,
        std::type_info const * info,
        qint64 requestTick
    );


public:
    optional<ThinkerPresentBase> tryGetDaemonPresent (
        methyl::Tree<Descriptor> descriptor,
        DaemonFactory factory,
        std::type_info const & info
    );

friend class DaemonBase;
protected:
    void afterThreadAttach (
        ThinkerBase & thinker,
        DaemonBase & daemon
    );

    void beforeThreadDetach (
        ThinkerBase & thinker,
        DaemonBase & daemon
    );

signals:
    void anyDaemonWritten ();

private:
    // can't use protected inheritance on QObjects with signal/slot
    void ensureThinkersPaused (codeplace const & cp) = delete;
    void ensureThinkersResumed (codeplace const & cp) = delete;

public:
    void ensureAllDaemonsPaused (codeplace const & cp);
    void ensureValidDaemonsResumed (codeplace const & cp);

    // This is how we get the actual pause requests to originate from
    // the Daemon Manager thread.  There is an assertion that all
    // ThinkerPresent objects are destroyed from the same thread that
    // created them... and since we need to do clean them up this
    // is how we must do it.  If that assertion is relaxed this may
    // not be necessary.

signals:
    void ensureDaemonsPausedBlocking (codeplace cp);
    void ensureDaemonsResumedBlocking (codeplace cp);

private slots:
    void onEnsureDaemonsPausedBlocking (codeplace cp);
    void onEnsureDaemonsResumedBlocking (codeplace cp);


#ifdef NEED_DAEMON_ENUMERATION
// Can enumerate thinkers, do we need this enumeration specifically?
private:
    void forAllDaemons(std::function<void(DaemonBase &)> fn) {
        for (auto & typeinfoAndMap : _daemonMap) {
            for (auto & nodeAndDaemon : typeinfoAndMap.second) {
                fn(*nodeAndDaemon.second);
            }
        }
    }
#endif


public:
    ~DaemonManager () override;
};


// Should these be static members of the DaemonManager?

bool isDaemonThreadCurrent();

bool isDaemonManagerThreadCurrent();

} // end namespace benzene

#endif
