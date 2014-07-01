//
// widget.h
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

#ifndef BENZENE_WIDGET_H
#define BENZENE_WIDGET_H

#include "methyl/defs.h"
#include "methyl/node.h"
#include "operation.h"
#include "daemon.h"
#include "hit.h"

#include <QtWidgets>

namespace benzene {

///////////////////////////////////////////////////////////////////////////////
//
// benzene::Widget
//
// This implements the fundamental behavior of a user interface element that
// follows the Benzene "philosophy", but can still participate in a normal
// QWidget layout.
//
// Mouse handling and painting are marked as "final" overrides.  A class
// deriving from benzene::Widget must defer to the notion that redraws are
// automatic and may happen as often as 30 frames per second, and that
// display reactions to user input must come in the form of "Operations" which
// are given as a parameter to the renderBenzene method.
//
//

class Widget : public QWidget {
    Q_OBJECT

public:
    Widget (QWidget * parent = 0, Qt::WindowFlags f = 0);

    ~Widget () override;

public:
    // This is the method you override in your widget to provide the
    // drawing behavior... don't use any GUI functions, only the QPainter!
    virtual void renderBenzene (
        QPainter & painter,
        OperationBase const * operation,
        OperationStatus status
    ) const = 0;

private:
    void paintEvent (QPaintEvent * event) final;

protected:
    // Not final, but you need to call it in your resize event!
    void resizeEvent (QResizeEvent * event);


private slots:
    void onBenzeneEvent (
        OperationBase const * operation,
        OperationStatus status
    );


signals:
    void renderedImage (QImage image);

private slots:
    void updatePixmap (QImage image);


public:
    virtual optional<methyl::RootNode<Hit>> makeHitForPoint (
        QPoint const & point
    ) const = 0;

private:
    void mouseMoveEvent (QMouseEvent * event) override final;

    void mousePressEvent (QMouseEvent * event) override final;

    void mouseReleaseEvent(QMouseEvent * event) override final;

    void enterEvent (QEvent * event) override final;

    void leaveEvent (QEvent * event) override final;


private:
    QPixmap _pixmap;

    bool _isLeftButtonDown;
};


} // end namespace benzene

#endif // BENZENE_WIDGET_H
