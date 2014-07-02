//
// application.cpp
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

#include <iostream>
#include <vector>

#include <QMessageBox>
#include <QThread>

#include "benzene/operationstatusbar.h"
#include "benzene/operation.h"

#include "worker.h"

using std::vector;

using methyl::Accessor;
using methyl::Node;
using methyl::Tree;
using methyl::NodePrivate;

using methyl::Context;

namespace benzene {


///////////////////////////////////////////////////////////////////////////////
//
// benzene::OperationProgressDialog
//
// For right now, the dialog which is shown during long-running operations is
// just a basic QProgressDialog.  The framework is not currently set up in
// such a way to have percentage progress notifications... although it could
// easily show something like the elapsed time.
//
// One idea in the original Benzene framework was to facilitate the termination
// of operations that were running too long, and to make restoring from the
// transaction log seem just like any other error from the user interface.
// This has been lost in the cross-platform transition to Qt, as the
// transparent relaunching of a process would involve a process API.
//

class OperationProgressDialog : public QProgressDialog
{
    Q_OBJECT

public:
    OperationProgressDialog (
        QString const & labelText,
        QString const & cancelButtonText,
        int minimum,
        int maximum,
        QWidget * parent = 0,
        Qt::WindowFlags f = 0
    ) :
        QProgressDialog (
            labelText, cancelButtonText, minimum, maximum, parent, f
        )
    {
        setWindowModality(Qt::WindowModal);

        // Hiding the close button doesn't seem to be an easy option; the
        // given techniques do not work, or cause erratic positioning.  So
        // instead we should give the close button behavior...or maybe
        // implement a custom dialog?
        //
        // http://stackoverflow.com/questions/16920412/

        // https://bugreports.qt-project.org/browse/QTBUG-26406
        QTimer::singleShot(1000, this, SLOT(show()));
    }

    ~OperationProgressDialog () override
    {
    }
};



///////////////////////////////////////////////////////////////////////////////
//
// benzene::ApplicationBase
//

ApplicationBase::ApplicationBase (int & argc, char ** argv) :
    ApplicationBase (argc, argv, []() { return true; } )
{
}

int const ApplicationBase::execResultInternal = 310556262;

void ApplicationBase::callExecAndCheckResult (
    QString const & phase,
    codeplace const & cp
) {
    Q_UNUSED(cp);

    int code = QApplication::exec();
    if (code == execResultInternal)
        return;

    QMessageBox msgBox;
    msgBox.setText(
        phase + "Failure, Exit Code: " + QString::number(code)
    );
    msgBox.setInformativeText(
        "There was an unexpected error in the Benzene Application Framework"
        " during a core phase which cannot be reported using mechanisms"
        " available to the framework when fully initialized.  Please report"
        " this error manually at the bug tracker:\n"
        "\n"
        "https://github.com/hostilefork/benzene/issues"
    );
    msgBox.setStandardButtons(QMessageBox::Close);
    msgBox.exec();

    // We don't want to signal success to the OS if a stray exit(0) ran...
    // so terminate with the internal result code.
    if (code == 0) {
        exit(execResultInternal);
    }

    exit(code);
}


ApplicationBase::ApplicationBase (
    int & argc,
    char ** argv,
    std::function<bool()> const & preamble
) :
    QApplication (argc, argv),
    _workerThread (new WorkerThread ())
{
    GUI

    hopefully(preamble(), HERE);

    qRegisterMetaType<shared_ptr<Context>>("shared_ptr<methyl::Context>");
    qRegisterMetaType<OperationStatus>("OperationStatus");

    // Before returning, we have to do some set up that may take a long time,
    // and the client's widgets may depend on this setup information.  So we
    // can't let them create any widgets until we've done this potentially
    // lengthy work.  The work must be done inside of our own temporary exec
    // loop, because we want to be able to have a progress bar and abort
    // button etc.
    //
    // So we send ourself a queued signal to kick off that work once we are
    // under the exec loop's stack.

    connect(
        this, &ApplicationBase::initialExecCall,
        this, &ApplicationBase::onInitialExecCall,
        Qt::QueuedConnection
    );

    emit initialExecCall();

    callExecAndCheckResult("startup", HERE);
}


void ApplicationBase::onInitialExecCall () {
    GUI

    // Here we know we are inside of the QApplication::exec() stack
    // (if that matters, which it might)

    _progress = make_unique<OperationProgressDialog>(
        "Initializing",
        "Abort",
        0,
        100,
        nullptr
    );

    connect(
        this, &ApplicationBase::initializeWorker,
        _workerThread.get(), &WorkerThread::initializeRequest
    );

    connect(
        _workerThread.get(), &WorkerThread::initializeComplete,
        this, &ApplicationBase::onWorkerInitializeComplete
    );

    emit initializeWorker();
}


Worker & ApplicationBase::getWorker () const {
    return _workerThread->getWorker();
}


WorkerThread & ApplicationBase::getWorkerThread () const {
    return *_workerThread;
}


void ApplicationBase::onWorkerInitializeComplete () {
    GUI

    _progress.reset();

    // Don't know really the best place to put these signal connections

    hopefully(connect(
        &getWorker(), &Worker::beginInvokeOperation,
        this, &ApplicationBase::onBeginInvokeOperation,
        Qt::QueuedConnection
    ), HERE);

    hopefully(connect(
        &getWorker(), &Worker::endInvokeOperation,
        this, &ApplicationBase::onEndInvokeOperation,
        Qt::QueuedConnection
    ), HERE);


    hopefully(connect(
        &getWorker(), &Worker::glancingOperation,
        this, &ApplicationBase::onGlancingOperation,
        Qt::QueuedConnection
    ), HERE);

    hopefully(connect(
        &getWorker(), &Worker::hoveringOperation,
        this, &ApplicationBase::onHoveringOperation,
        Qt::QueuedConnection
    ), HERE);

    hopefully(connect(
        &getWorker(), &Worker::pendingOperation,
        this, &ApplicationBase::onPendingOperation,
        Qt::QueuedConnection
    ), HERE);

    hopefully(connect(
        &getWorker(), &Worker::nullOperation,
        this, &ApplicationBase::onNullOperation,
        Qt::QueuedConnection
    ), HERE);


    connect(
        this, &ApplicationBase::glanceHit,
        &getWorker(), &Worker::receiveGlanceHit
    );

    connect(
        this, &ApplicationBase::firstHit,
        &getWorker(), &Worker::receiveFirstHit
    );

    connect(
        this, &ApplicationBase::nextHit,
        &getWorker(), &Worker::receiveNextHit
    );

    connect(
        this, &ApplicationBase::lastHit,
        &getWorker(), &Worker::receiveLastHit
    );

    // first time we'll exit the ::exec() loop
    exit(execResultInternal);
}


int ApplicationBase::exec (QWidget & mainWidget) {
    GUI

    // This is the overridden exec() that the ApplicationBase client
    // explicitly invokes.

    // We want to know what the main Widget is to put progress dialogs on
    // etc.  See if there is a better way to attack that problem?  Perhaps
    // the top-level parent of the BenzeneStatusBar could be implicitly that
    // widget.  Yet it's a larger problem if one has multiple top-level
    // windows.

    getWorker().setMainWidget(mainWidget);

    // Initial draw.  We should actually get the mouse location; because
    // this means the mouse will have to move to get a hover effect if
    // they happen to have landed on something hoverable after initialization
    emitGlanceHit(nullopt);

    int result = QApplication::exec();

    // We have to sync up the worker so that all render calls are finished
    // before we start trying to destruct the application object.  This shuts
    // down the worker's event loop but does not destroy Worker *yet*!

    connect(
        this, &ApplicationBase::exitWorkerLoop,
        _workerThread.get(), &WorkerThread::exitLoopRequest,
        Qt::QueuedConnection
    );

    connect(
        _workerThread.get(), &WorkerThread::exitLoopComplete,
        this, &ApplicationBase::onWorkerLoopExited,
        Qt::QueuedConnection
    );

    emit exitWorkerLoop();

    callExecAndCheckResult("exitWorkerLoop", HERE);

    return result;
}


void ApplicationBase::onWorkerLoopExited () {

    ApplicationBase & app = *this;

    // Could have used quit() slot, but this makes it easier to debug; and
    // also lets us ensure that the exit is being caused by what we think

    exit(execResultInternal);
}


void ApplicationBase::emitGlanceHit (
    optional<Tree<Hit>> && hit
) const
{
    GUI

    unique_ptr<NodePrivate> nodePrivateOwned;
    shared_ptr<Context> context;

    if (hit) {
        std::tie(nodePrivateOwned, context)
            = methyl::globalEngine->dissectTree(std::move(*hit));
        hit = nullopt;
        emit glanceHit(nodePrivateOwned.release(), context);
    } else {
        emit glanceHit(nullptr, nullptr);
    }
}


void ApplicationBase::emitFirstHit(optional<Tree<Hit>> && hit) const {
    GUI

    unique_ptr<NodePrivate> nodePrivateOwned;
    shared_ptr<Context> context;

    if (hit) {
        std::tie(nodePrivateOwned, context)
            = methyl::globalEngine->dissectTree(std::move(*hit));
        hit = nullopt;
        emit firstHit(nodePrivateOwned.release(), context);
    } else {
        emit firstHit(nullptr, nullptr);
    }
}


void ApplicationBase::emitNextHit(optional<Tree<Hit>> && hit) const {
    GUI

    unique_ptr<NodePrivate> nodePrivateOwned;
    shared_ptr<Context> context;

    if (hit) {
        std::tie(nodePrivateOwned, context)
            = methyl::globalEngine->dissectTree(std::move(*hit));
        hit = nullopt;
        emit nextHit(nodePrivateOwned.release(), context);
    } else {
        emit nextHit(nullptr, nullptr);
    }
}


void ApplicationBase::emitLastHit (optional<Tree<Hit>> && hit) const {
    GUI

    unique_ptr<NodePrivate> nodePrivateOwned;
    shared_ptr<Context> context;

    if (hit) {
        std::tie(nodePrivateOwned, context)
            = methyl::globalEngine->dissectTree(std::move(*hit));
        hit = nullopt;
        emit lastHit(nodePrivateOwned.release(), context);
    } else {
        emit lastHit(nullptr, nullptr);
    }
}


auto ApplicationBase::operationForPress (
    Node<Hit const> const & hit
) const
    -> optional<unique_ptr<OperationBase>>
{
    WORKER

    Q_UNUSED(hit);
    return nullopt;
}


auto ApplicationBase::operationForRepress (
    Node<Hit const> const & hit
) const
    -> optional<unique_ptr<OperationBase>>
{
    WORKER

    Q_UNUSED(hit);
    return nullopt;
}


auto ApplicationBase::operationForStroke (
    std::vector<optional<Node<Hit const>>> const & hitList
) const
    -> optional<unique_ptr<OperationBase>>
{
    WORKER

    Q_UNUSED(hitList);
    return nullopt;
}



auto ApplicationBase::operationForLine (
    Node<Hit const> const & startHit,
    Node<Hit const> const & endHit
) const
    -> optional<unique_ptr<OperationBase>>
{
    WORKER

    Q_UNUSED(startHit);
    Q_UNUSED(endHit);
    return nullopt;
}


void ApplicationBase::queueInvokeOperationMaybe (
    unique_ptr<OperationBase> && operation
) const
{
    emit getWorker().queueInvokeOperationMaybe(operation.release());
}


Node<Accessor const> ApplicationBase::getDocument () const {

    hopefully(not isDaemonThreadCurrent(), HERE);

    // REVIEW: What context should be given for read only access to
    // the document from arbitrary threads?

    return methyl::globalEngine->contextualNodeRef(
        (*getWorker()._document).root(),
        methyl::globalEngine->contextForLookup()
    );
}


void ApplicationBase::onBeginInvokeOperation (QString const & message) {

    GUI

    for (OperationStatusBar * statusBar : _statusBars) {
        QImage hourglassImage (":/silk/hourglass.png");
        statusBar->showOperationStatus(
            OperationStatus::Running, message
        );
    }

    _progress = make_unique<OperationProgressDialog>(
        message,
        "Abort",
        0,
        100,
        &getWorker().getMainWidget()
    );
}


void ApplicationBase::onEndInvokeOperation (
    bool success,
    QString const & message
) {

    GUI

    _progress.reset();

    for (OperationStatusBar * statusBar : _statusBars) {
        if (success) {
            statusBar->showInformation(message);
        } else {
            statusBar->showError(message);
        }
    }
}


void ApplicationBase::onGlancingOperation (QString message) {
    GUI

    // There is a 16x16 status bar icon registered for "glancing" as an
    // eye, but it's kind of disruptive.  For now, we don't let it
    // interact with the status bar.

#ifdef GIVE_STATUSBAR_FEEDBACK_ON_GLANCING
    for (OperationStatusBar * statusBar : _statusBars) {
        statusBar->showOperationStatus(OperationStatus::Glancing, message);
    }
#endif
}


void ApplicationBase::onHoveringOperation (QString message) {
    GUI

    for (OperationStatusBar * statusBar : _statusBars) {
        statusBar->showOperationStatus(OperationStatus::Hovering, message);
    }
}


void ApplicationBase::onPendingOperation (QString message) {
    GUI

    for (OperationStatusBar * statusBar : _statusBars) {
        statusBar->showOperationStatus(OperationStatus::Pending, message);
    }
}


void ApplicationBase::onNullOperation () {
    GUI

    for (OperationStatusBar * statusBar : _statusBars) {
        statusBar->clearMessage();
    }
}



void ApplicationBase::onFinalExecCall () {

    GUI

    _progress = make_unique<OperationProgressDialog>(
        "Can shut down more cleanly if you wait a second...",
        "Nope",
        0,
        100,
        nullptr
    );

    // We don't actually quit on the first time the event loop is exited
    // We start another event loop, which will shut itself down
    // This way we can have an exiting progress dialog.

    // We don't use a Qt::BlockingQueuedConnection here because we want
    // to keep the GUI responsive--among other reasons, so that we can
    // respond to the timer request to display a progress dialog.
    connect(
        this, &ApplicationBase::shutdownWorker,
        _workerThread.get(), &WorkerThread::shutdownRequest
    );

    connect(
        _workerThread.get(), &WorkerThread::shutdownComplete,
        this, &ApplicationBase::onWorkerShutdownComplete
    );

    emit shutdownWorker();
}


void ApplicationBase::onWorkerShutdownComplete () {
    GUI

    // final time we'll exit the ::exec() loop
    exit(execResultInternal);

    _progress.reset();
}


ApplicationBase::~ApplicationBase () {
    GUI

    // As with the initialization, we have a shutdown that may take an
    // arbitrary amount of time.  Once again send self a signal so we can
    // do processing that requires the exec loop to have been entered; by
    // using a QueuedConnection

    connect(
        this, &ApplicationBase::finalExecCall,
        this, &ApplicationBase::onFinalExecCall,
        Qt::QueuedConnection
    );

    emit finalExecCall();

    callExecAndCheckResult("shutdown", HERE);
}

} // end namespace benzene


// https://qt-project.org/forums/viewthread/12689
#include "benzeneapplication.moc"
