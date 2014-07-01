//
// widget.cpp
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

#include "methyl/node.h"
#include "worker.h"

#include "benzene/widget.h"

using methyl::NodePrivate;
using methyl::Node;
using methyl::RootNode;
using methyl::NodeRef;

using methyl::Context;

namespace benzene {

Widget::Widget (QWidget * parent, Qt::WindowFlags f) :
    QWidget (parent, f),
    _isLeftButtonDown (false)
{
    GUI

    auto & app = getApplication<ApplicationBase>();

    connect(
        &app, &ApplicationBase::benzeneEvent,
        this, &Widget::onBenzeneEvent,
        // IMPORTANT!  Widgets want WORKER thread for render
        Qt::DirectConnection
    );

    connect(
        this, &Widget::renderedImage,
        this, &Widget::updatePixmap,
        // IMPORTANT!  Calling update/paintEvent on GUI from WORKER
        Qt::QueuedConnection
    );

    // By default Qt does not send you mouse messages unless a button is
    // pressed; turning mouse tracking on lets us do hover events etc.
    setMouseTracking(true);
}


void Widget::onBenzeneEvent (
    OperationBase const * operation,
    OperationStatus status
) {
    WORKER

    // REVIEW: is it safe to ask about the window's size here?
    QSize size = rect().size();
    QImage image(size, QImage::Format_RGB32);
    QPainter painter (&image);

    this->renderBenzene(painter, operation, status);

    emit renderedImage(image);
}


void Widget::updatePixmap (QImage image) {
    GUI

    // Note that the image is an implicitly shared type.  Hence, passing it
    // by value over a signal/slot will not incur a copy if the image is
    // not written to by another thread.

    _pixmap = QPixmap::fromImage(image);

    // Even though we're on the GUI thread, we have to call update() because
    // you can't draw the widget outside of a paintEvent()

    update();
}


void Widget::paintEvent (QPaintEvent * event) {
    GUI

    // Qt normally doesn't allow us to paint outside of
    // paintEvent(). In particular, we can't paint from the
    // mouse event handlers. (This behavior can be changed
    // using the Qt::WA_PaintOnScreen widget attribute, though.)
    //
    // The framework invokes the render function here with
    // the appropriate parameters.  Details of the paint event
    // are not available; full painting must be done.

    Q_UNUSED(event);

    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    if (_pixmap.isNull()) {
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter,
            tr("Rendering initial image, please wait...")
        );
        return;
    }

    painter.drawPixmap(QPoint(0, 0), _pixmap);
}


void Widget::resizeEvent (QResizeEvent * event) {
    GUI

    QSize oldSize = event->oldSize();
    Q_UNUSED(oldSize);

    // If you resized the window, we'll call that a null glance to get the
    // repainting done.

    auto & app = getApplication<ApplicationBase>();

    if (_isLeftButtonDown) {
        // Can get out of whack if there's alt-tabbing or other
        // weirdness.  I'm washing my hands of it and just cleaning
        // up if I notice this being the case.
        _isLeftButtonDown = false;
    }

    app.emitGlanceHit(nullopt);
}


void Widget::mouseMoveEvent (QMouseEvent * event) {
    GUI

    auto & app = getApplication<ApplicationBase>();

    if (_isLeftButtonDown and not (event->buttons() & Qt::LeftButton)) {
        // Can get out of whack if there's alt-tabbing or other
        // weirdness.  I'm washing my hands of it and just cleaning
        // up if I notice this being the case.
        _isLeftButtonDown = false;
    }

    auto hit = makeHitForPoint(event->pos());

    if(_isLeftButtonDown) {
        app.emitNextHit(std::move(hit));
    } else {
        app.emitGlanceHit(std::move(hit));
    }
}


void Widget::mousePressEvent (QMouseEvent * event) {
    GUI

    auto & app = getApplication<ApplicationBase>();

    auto hit = makeHitForPoint(event->pos());

    if (event->button() == Qt::LeftButton) {
        _isLeftButtonDown = true;
        app.emitFirstHit(std::move(hit));
        return;
    }

    if (_isLeftButtonDown and not (event->buttons() & Qt::LeftButton)) {
        // Can get out of whack if there's alt-tabbing or other
        // weirdness.  I'm washing my hands of it and just cleaning
        // up if I notice this being the case.
        _isLeftButtonDown = false;
    }

    // Left button was down when another button was pressed
    if (_isLeftButtonDown) {
        app.emitNextHit(std::move(hit));
    } else {
        app.emitFirstHit(nullopt);
    }
}


void Widget::mouseReleaseEvent (QMouseEvent * event) {
    GUI

    auto & app = getApplication<ApplicationBase>();

    auto hit = makeHitForPoint(event->pos());

    if(_isLeftButtonDown and (event->button() == Qt::LeftButton)) {
        _isLeftButtonDown = false;
        app.emitLastHit(std::move(hit));
        return;
    }

    if (_isLeftButtonDown and not (event->buttons() & Qt::LeftButton)) {
        // Can get out of whack if there's alt-tabbing or other
        // weirdness.  I'm washing my hands of it and just cleaning
        // up if I notice this being the case.
        _isLeftButtonDown = false;
    } else {
        app.emitFirstHit(std::move(hit));
    }
}


void Widget::enterEvent (QEvent * event) {
    GUI

    // Do nothing?
}


void Widget::leaveEvent (QEvent * event) {
    GUI

    auto & app = getApplication<ApplicationBase>();

    if(_isLeftButtonDown) {
        app.emitNextHit(nullopt);
        return;
    }

    app.emitGlanceHit(nullopt);
}


Widget::~Widget () {
}


} // end namespace benzene
