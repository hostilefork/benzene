//
// application.h
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

#ifndef BENZENE_APPLICATION_H
#define BENZENE_APPLICATION_H

#include <unordered_set>

#include <QApplication>
#include <QtWidgets>

#include "methyl/defs.h"

#include "benzene/hit.h"


namespace benzene {

// REVIEW: Getting typed templated access to the Application subtype
// in "operation.h" complicates making it an include of this header as
// opposed to vice-versa.

class OperationBase;

enum class OperationStatus {
    None,

    // Hover timer period hasn't elapsed
    Glancing,

    // Hover timer has elapsed but "button isn't pressed"
    Hovering,

    // "Button is pressed" but operation is not committed
    Pending,

    // "Button release" signifies that operation is to be run
    Running
};

class WorkerThread;

class Worker;

class OperationStatusBar;

class RunDialog;


///////////////////////////////////////////////////////////////////////////////
//
// benzene::ApplicationBase
//
// Benzene tries to abstract the Qt framework notion of an application, and
// actually derives from QApplication.  It goes further in suggesting that
// you actually make *your* singleton application instance derive from the
// Benzene application type.  This would be like Qt telling you to write:
//
//     class MyApplication : public QApplication
//     {
//     public:
//         MyApplication (int argc, char * argv []) :
//             QApplication (argc, argv)
//         { ... }
//     private:
//         // application global state
//         ...
//     };
//
// That isn't how Qt tutorial code is written, although even Qt layers
// application types (QApplication derives from QCoreApplication, for instance)
// and provides virtual methods.  So it's certainly possible.  See:
//
//     https://github.com/hostilefork/benzene/issues/8
//

class ApplicationBase : public QApplication {

    Q_OBJECT

public:
    // "Warning: The data referred to by argc and argv must stay valid for
    // the entire lifetime of the QApplication object. In addition, argc
    // must be greater than zero and argv must contain at least one valid
    // character string."

    ApplicationBase (int & argc, char ** argv);

    // An enhanced constructor is available if you have code you want to
    // run after the baseline QApplication construction is complete, but
    // before the Benzene initialization begins.  This "preamble" is run
    // on the GUI thread, and if it returns false then the application
    // will terminate.  Use this to do things like setWindowIcon or
    // setting themes which you need to happen before Benzene starts
    // making pop up windows using the information.

    ApplicationBase (
        int & argc,
        char ** argv,
        std::function<bool()> const & preamble
    );

    ~ApplicationBase () override;


private:
    // The benzene::WorkerThread spawns the benzene::Worker, which holds much
    // of the framework state and handles most of the interaction with client
    // code for the framework.

    unique_ptr<WorkerThread> _workerThread;

friend class DaemonManager;
friend class OperationBase;
friend class DaemonBase;
friend bool isDaemonManagerThreadCurrent ();
private:
    Worker & getWorker () const;

friend bool isWorkerThreadCurrent ();
private:
    WorkerThread & getWorkerThread () const;

signals:
    void initializeWorker ();

    void exitWorkerLoop ();

    void shutdownWorker ();

private slots:
    void onWorkerInitializeComplete ();

    void onWorkerLoopExited ();

    void onWorkerShutdownComplete ();


private:
    // One advantage of having a Worker/GUI separation is the ability to keep
    // the user interface responsive, even during what might turn into a long
    // operation on the worker.  By keeping the GUI loop free, we are able
    // to set a timer and bring up a progress dialog if any operation takes
    // too long.  (That includes startup and shutdown of the system.)

    unique_ptr<RunDialog> _runDialog;


private:
    // If we are exiting our own loops, we send this number instead of "0"
    // which quit() or exit(0) would send to make sure the loop was exited
    // at the location and for the reason that we thought!

    static int const execResultInternal;

    int exec () = delete;

    void callExecAndCheckResult (
        QString const & phase,
        codeplace const & cp
    );


private slots:
    void onInitialExecCall ();

    void onFinalExecCall ();

signals:
    void initialExecCall ();

    void finalExecCall ();

public:
    int exec (QWidget & mainWidget);


public:
    methyl::Node<methyl::Accessor const> getDocument() const;


    // Originally benzeneEvent was in an interface that could be multiply
    // inherited from.  But the inability to use QObject as a virtual base
    // meant there could be no signals or slots; limiting its usefulness.
    //
    //     http://stackoverflow.com/q/19129133
    //
    // Consequently this is a benzeneEvent.  If your widget wants this event
    // it will have to connect to it.  Note connecting directly will issue
    // events on the ENGINE thread; use a QueuedConnection (default) if you are
    // expecting to do GUI work when you receive it!
signals:
    void benzeneEvent (
        OperationBase const * operation,
        OperationStatus status
    );


signals:
    // Signals aren't designed to work with move-only types, nor can they
    // be private.  We extract the internal node and pass it.

    void glanceHit (
        methyl::NodePrivate * rootHit, shared_ptr<methyl::Context> context
    ) const;

    void firstHit (
        methyl::NodePrivate * rootHit, shared_ptr<methyl::Context> context
    ) const;

    void nextHit (
        methyl::NodePrivate * rootHit, shared_ptr<methyl::Context> context
    ) const;

    void lastHit (
        methyl::NodePrivate * rootHit, shared_ptr<methyl::Context> context
    ) const;

public:
    void emitGlanceHit (optional<methyl::Tree<Hit>> && hit) const;

    void emitFirstHit (optional<methyl::Tree<Hit>> && hit) const;

    void emitNextHit (optional<methyl::Tree<Hit>> && hit) const;

    void emitLastHit (optional<methyl::Tree<Hit>> && hit) const;


friend class OperationStatusBar;
private:
    std::unordered_set<OperationStatusBar *> _statusBars;

    void addStatusBar (OperationStatusBar & statusBar) {
        _statusBars.insert(&statusBar);
    }

    void removeStatusBar (OperationStatusBar & statusBar) {
        _statusBars.erase(&statusBar);
    }


public:
    virtual optional<unique_ptr<OperationBase>> operationForPress (
        methyl::Node<Hit const> const & hit
    ) const;

    virtual optional<unique_ptr<OperationBase>> operationForRepress (
        methyl::Node<Hit const> const & hit
    ) const;

    virtual optional<unique_ptr<OperationBase>> operationForStroke (
        std::vector<optional<methyl::Node<Hit const>>> const & hitList
    ) const;

    virtual optional<unique_ptr<OperationBase>> operationForLine (
        methyl::Node<Hit const> const & startHit,
        methyl::Node<Hit const> const & endHit
    ) const;

public:
    void queueInvokeOperationMaybe (
        unique_ptr<OperationBase> && operation
    ) const;

private slots:
    void onBeginInvokeOperation (QString const & message);

    void onEndInvokeOperation (bool success, QString const & message);

    void onHoveringOperation (QString message);

    void onGlancingOperation (QString message);

    void onPendingOperation (QString message);

    void onNullOperation ();
};



///////////////////////////////////////////////////////////////////////////////
//
// benzene::Application<T>
//
// Templated class you should use as the base class for your new Benzene
// application type.  The parameter is the Accessoraccessor class that is
// used to represent documents.
//
// Because BenzeneApplication is derived from QApplication, you should put
// it at the place in your program where you would usually put a QApplication;
// generally in your main.cpp file.  And just as how a QApplication
// (or QCoreApplication) must be instantiated before using any Qt functions,
// so must a Benzene Application be instantiated before any Benzene functions
// are invoked.
//
// Due to the singleton status of the Benzene Application, it can be a good
// place to store application global state (to the extent that having
// lots of application global state is a good idea in the first place).
// You can get the properly-typed version of your derived class with the
// global accessor getApplication<ApplicationType>().
//

template <class T>
class Application : public ApplicationBase {

public:
    typedef T NodeType;

public:
    template <typename... Args>
    Application (Args&&... args) :
        ApplicationBase (std::forward<Args>(args)...)
    {
    }

public:
    static methyl::Node<T const> getDocument () {
        auto app = dynamic_cast<ApplicationBase *>(
            QApplication::instance()
        );
        // REVIEW: Should we check?  Dereference may be unsafe.
        return *methyl::Node<T>::checked(app->getDocument());
    }

public:
    ~Application () override {
    }
};


// operator<< for OperationStatus
//
// Required by tracked<T> because it must be able to present a proper debug
// message about the tracked value.

inline QTextStream & operator<< (
    QTextStream & o,
    OperationStatus const & status
) {
    o << "OperationStatus::";
    switch (status) {
    case OperationStatus::None:
        o << "None";
        break;
    case OperationStatus::Glancing:
        o << "Glancing";
        break;
    case OperationStatus::Hovering:
        o << "Hovering";
        break;
    case OperationStatus::Pending:
        o << "Pending";
        break;
    case OperationStatus::Running:
        o << "Running";
        break;
    default:
        hopefullyNotReached(HERE);
    }
    return o;
}


// Parallel to QApplication's instance()
// http://qt-project.org/doc/qt-5/QCoreApplication.html

template <class T>
T const & getApplication() {
    return *dynamic_cast<T const *>(T::instance());
}


// These internal checks can be helpful in client code as well

bool hopefullyWorkerThreadCurrent (codeplace const & cp);

bool hopefullyGuiThreadCurrent (codeplace const & cp);

bool hopefullyDaemonThreadCurrent (codeplace const & cp);

bool hopefullyDaemonManagerThreadCurrent (codeplace const & cp);

} // end namespace benzene



#define WORKER benzene::hopefullyWorkerThreadCurrent(HERE);

#define GUI benzene::hopefullyGuiThreadCurrent(HERE);

#define DAEMON benzene::hopefullyDaemonThreadCurrent(HERE);

#define DAEMONMANAGER benzene::hopefullyDaemonManagerThreadCurrent(HERE);


#endif // BENZENE_APPLICATION_H
