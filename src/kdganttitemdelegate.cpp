/****************************************************************************
 ** Copyright (C) 2001-2006 Klarälvdalens Datakonsult AB.  All rights reserved.
 **
 ** This file is part of the KD Gantt library.
 **
 ** This file may be distributed and/or modified under the terms of the
 ** GNU General Public License version 2 as published by the Free Software
 ** Foundation and appearing in the file LICENSE.GPL included in the
 ** packaging of this file.
 **
 ** Licensees holding valid commercial KD Gantt licenses may use this file in
 ** accordance with the KD Gantt Commercial License Agreement provided with
 ** the Software.
 **
 ** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 ** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 **
 ** See http://www.kdab.net/kdgantt for
 **   information about KD Gantt Commercial License Agreements.
 **
 ** Contact info@kdab.net if any conditions of this
 ** licensing are not clear to you.
 **
 **********************************************************************/
#include "kdganttitemdelegate_p.h"
#include "kdganttglobal.h"
#include "kdganttstyleoptionganttitem.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QModelIndex>
#include <QAbstractItemModel>
#include <QApplication>

#ifndef QT_NO_DEBUG_STREAM

#define PRINT_INTERACTIONSTATE(x) \
    case x: dbg << #x; break;

QDebug operator<<(QDebug dbg, KDGantt::ItemDelegate::InteractionState state)
{
    switch (state) {
        PRINT_INTERACTIONSTATE(KDGantt::ItemDelegate::State_None);
        PRINT_INTERACTIONSTATE(KDGantt::ItemDelegate::State_Move);
        PRINT_INTERACTIONSTATE(KDGantt::ItemDelegate::State_ExtendLeft);
        PRINT_INTERACTIONSTATE(KDGantt::ItemDelegate::State_ExtendRight);
    default:
        break;
    }
    return dbg;
}

#undef PRINT_INTERACTIONSTATE

#endif /* QT_NO_DEBUG_STREAM */

using namespace KDGantt;

/*!\class KDGantt::ItemDelegate kdganttitemdelegate.h KDGanttItemDelegate
 *\ingroup KDGantt
 *\brief Class used to render gantt items in a KDGantt::GraphicsView
 *
 */

/*!\enum KDGantt::ItemDelegate::InteractionState
 * This enum is used for communication between the view and
 * the delegate about user interaction with gantt items.
 *
 * \see KDGantt::ItemDelegate::interactionStateFor
 */

ItemDelegate::Private::Private()
{
    // Brushes
    QLinearGradient taskgrad(0., 0., 0., QApplication::fontMetrics().height());
    taskgrad.setColorAt(0., Qt::green);
    taskgrad.setColorAt(1., Qt::darkGreen);

    QLinearGradient summarygrad(0., 0., 0., QApplication::fontMetrics().height());
    summarygrad.setColorAt(0., Qt::blue);
    summarygrad.setColorAt(1., Qt::darkBlue);

    QLinearGradient eventgrad(0., 0., 0., QApplication::fontMetrics().height());
    eventgrad.setColorAt(0., Qt::red);
    eventgrad.setColorAt(1., Qt::darkRed);

    defaultbrush[TypeTask]    = taskgrad;
    defaultbrush[TypeSummary] = summarygrad;
    defaultbrush[TypeEvent]   = eventgrad;

    // Pens
    QPen pen(Qt::black, 1.);

    defaultpen[TypeTask]    = pen;
    defaultpen[TypeSummary] = pen;
    defaultpen[TypeEvent]   = pen;
}

/*! Constructor. Creates an ItemDelegate with parent \a parent */
ItemDelegate::ItemDelegate(QObject *parent)
    : QItemDelegate(parent), _d(new Private)
{
}

/*! Destructor */
ItemDelegate::~ItemDelegate()
{
    delete _d;
}

#define d d_func()

/*! Sets the default brush used for items of type \a type to
 * \a brush. The default brush is used in the case when the model
 * does not provide an explicit brush.
 *
 * \todo Move this to GraphicsView to make delegate stateless.
 */
void ItemDelegate::setDefaultBrush(ItemType type, const QBrush &brush)
{
    d->defaultbrush[type] = brush;
}

/*!\returns The default brush for item type \a type
 *
 * \todo Move this to GraphicsView to make delegate stateless.
 */
QBrush ItemDelegate::defaultBrush(ItemType type) const
{
    return d->defaultbrush[type];
}

/*! Sets the default pen used for items of type \a type to
 * \a pen. The default pen is used in the case when the model
 * does not provide an explicit pen.
 *
 * \todo Move this to GraphicsView to make delegate stateless.
 */
void ItemDelegate::setDefaultPen(ItemType type, const QPen &pen)
{
    d->defaultpen[type] = pen;
}

/*!\returns The default pen for item type \a type
 *
 * \todo Move this to GraphicsView to make delegate stateless.
 */
QPen ItemDelegate::defaultPen(ItemType type) const
{
    return d->defaultpen[type];
}

/*! \returns The bounding Span for the item identified by \a idx
 * when rendered with options \a opt. This is often the same as the
 * span given by the AbstractGrid for \a idx, but it might be larger
 * in case there are additional texts or decorations on the item.
 *
 * Override this to implement new itemtypes or to change the look
 * of the existing ones.
 */
Span ItemDelegate::itemBoundingSpan(const StyleOptionGanttItem &opt,
                                    const QModelIndex &idx) const
{
    if (!idx.isValid()) {
        return Span();
    }

    const QString txt = idx.model()->data(idx, Qt::DisplayRole).toString();
    const int typ = idx.model()->data(idx, ItemTypeRole).toInt();
    QRectF itemRect = opt.itemRect;

    if (typ == TypeEvent) {
        itemRect = QRectF(itemRect.left() - itemRect.height() / 2.,
                          itemRect.top(),
                          itemRect.height(),
                          itemRect.height());
    }

    int tw = opt.fontMetrics.width(txt);
    tw += static_cast<int>(itemRect.height() / 2.);
    Span s;
    switch (opt.displayPosition) {
    case StyleOptionGanttItem::Left:
        s = Span(itemRect.left() - tw, itemRect.width() + tw); break;
    case StyleOptionGanttItem::Right:
        s = Span(itemRect.left(), itemRect.width() + tw); break;
    case StyleOptionGanttItem::Center:
        s = Span(itemRect.left(), itemRect.width()); break;
    }
    return s;
}

/*! \returns The interaction state for position \a pos on item \a idx
 * when rendered with options \a opt. This is used to tell the view
 * about how the item should react to mouse click/drag.
 *
 * Override to implement new items or interactions.
 */
ItemDelegate::InteractionState ItemDelegate::interactionStateFor(const QPointF &pos,
        const StyleOptionGanttItem &opt,
        const QModelIndex &idx) const
{
    if (!idx.isValid()) {
        return State_None;
    }
    if (!(idx.model()->flags(idx) & Qt::ItemIsEditable)) {
        return State_None;
    }

    const int typ = static_cast<ItemType>(idx.model()->data(idx, ItemTypeRole).toInt());
    if (typ == TypeNone || typ == TypeSummary) {
        return State_None;
    }
    if (typ == TypeEvent) {
        return State_Move;
    }
    if (!opt.itemRect.contains(pos)) {
        return State_None;
    }

    qreal delta = 5.;
    if (opt.itemRect.width() < 15) {
        delta = 1.;
    }
    if (pos.x() >= opt.itemRect.left() && pos.x() < opt.itemRect.left() + delta) {
        return State_ExtendLeft;
    } else   if (pos.x() <= opt.itemRect.right() && pos.x() > opt.itemRect.right() - delta) {
        return State_ExtendRight;
    } else {
        return State_Move;
    }
}

/*! Paints the gantt item \a idx using \a painter and \a opt
 */
void ItemDelegate::paintGanttItem(QPainter *painter,
                                  const StyleOptionGanttItem &opt,
                                  const QModelIndex &idx)
{
    if (!idx.isValid()) {
        return;
    }
    const ItemType typ = static_cast<ItemType>(idx.model()->data(idx, ItemTypeRole).toInt());
    const QString &txt = opt.text;
    QRectF itemRect = opt.itemRect;
    QRectF boundingRect = opt.boundingRect;
    boundingRect.setY(itemRect.y());
    boundingRect.setHeight(itemRect.height());

    //qCDebug(KDGANTT_LOG) << "itemRect="<<itemRect<<", boundingRect="<<boundingRect;
    //qCDebug(KDGANTT_LOG) << painter->font() << opt.fontMetrics.height() << painter->device()->width() << painter->device()->height();

    painter->save();

    QPen pen = defaultPen(typ);
    if (opt.state & QStyle::State_Selected) {
        pen.setWidth(2 * pen.width());
    }
    painter->setPen(pen);
    painter->setBrush(defaultBrush(typ));

    qreal pw = painter->pen().width() / 2.;
    switch (typ) {
    case TypeTask:
        if (itemRect.isValid()) {
            // TODO
            qreal pw = painter->pen().width() / 2.;
            pw -= 1;
            QRectF r = itemRect;
            r.translate(0., r.height() / 6.);
            r.setHeight(2.*r.height() / 3.);
            painter->setBrushOrigin(itemRect.topLeft());
            painter->save();
            painter->translate(0.5, 0.5);
            painter->drawRect(r);
            bool ok;
            qreal completion = idx.model()->data(idx, KDGantt::TaskCompletionRole).toDouble(&ok);
            if (ok) {
                qreal h = r.height();
                QRectF cr(r.x(), r.y() + h / 4.,
                          r.width()*completion / 100., h / 2. + 1 /*??*/);
                QColor compcolor(painter->pen().color());
                compcolor.setAlpha(150);
                painter->fillRect(cr, compcolor);
            }
            painter->restore();
            Qt::Alignment ta;
            switch (opt.displayPosition) {
            case StyleOptionGanttItem::Left: ta = Qt::AlignLeft; break;
            case StyleOptionGanttItem::Right: ta = Qt::AlignRight; break;
            case StyleOptionGanttItem::Center: ta = Qt::AlignCenter; break;
            }
            painter->drawText(boundingRect, ta | Qt::AlignVCenter, txt);
        }
        break;
    case TypeSummary:
        if (opt.itemRect.isValid()) {
            // TODO
            pw -= 1;
            const QRectF r = QRectF(opt.itemRect).adjusted(-pw, -pw, pw, pw);
            QPainterPath path;
            const qreal delta = r.height() / 2.;
            path.moveTo(r.topLeft());
            path.lineTo(r.topRight());
            path.lineTo(QPointF(r.right(), r.top() + 2.*delta));
            //path.lineTo( QPointF( r.right()-3./2.*delta, r.top() + delta ) );
            path.quadTo(QPointF(r.right() - .5 * delta, r.top() + delta), QPointF(r.right() - 2.*delta, r.top() + delta));
            //path.lineTo( QPointF( r.left()+3./2.*delta, r.top() + delta ) );
            path.lineTo(QPointF(r.left() + 2.*delta, r.top() + delta));
            path.quadTo(QPointF(r.left() + .5 * delta, r.top() + delta), QPointF(r.left(), r.top() + 2.*delta));
            path.closeSubpath();
            painter->setBrushOrigin(itemRect.topLeft());
            painter->save();
            painter->translate(0.5, 0.5);
            painter->drawPath(path);
            painter->restore();
            Qt::Alignment ta;
            switch (opt.displayPosition) {
            case StyleOptionGanttItem::Left: ta = Qt::AlignLeft; break;
            case StyleOptionGanttItem::Right: ta = Qt::AlignRight; break;
            case StyleOptionGanttItem::Center: ta = Qt::AlignCenter; break;
            }
            painter->drawText(boundingRect, ta | Qt::AlignVCenter, txt);
        }
        break;
    case TypeEvent: /* TODO */
        //qCDebug(KDGANTT_LOG) << opt.boundingRect << opt.itemRect;
        if (opt.boundingRect.isValid()) {
            const qreal pw = painter->pen().width() / 2. - 1;
            const QRectF r = QRectF(opt.itemRect).adjusted(-pw, -pw, pw, pw).translated(-opt.itemRect.height() / 2, 0);
            QPainterPath path;
            const qreal delta = static_cast< int >(r.height() / 2);
            path.moveTo(delta, 0.);
            path.lineTo(2.*delta, delta);
            path.lineTo(delta, 2.*delta);
            path.lineTo(0., delta);
            path.closeSubpath();
            painter->save();
            painter->translate(r.topLeft());
            painter->translate(0, 0.5);
            painter->drawPath(path);
            painter->restore();
            Qt::Alignment ta;
            switch (opt.displayPosition) {
            case StyleOptionGanttItem::Left: ta = Qt::AlignLeft; break;
            case StyleOptionGanttItem::Right: ta = Qt::AlignRight; break;
            case StyleOptionGanttItem::Center: ta = Qt::AlignCenter; break;
            }
            painter->drawText(boundingRect, ta | Qt::AlignVCenter, txt);
#if 0
            painter->setBrush(Qt::NoBrush);
            painter->setPen(Qt::black);
            painter->drawRect(opt.boundingRect);
            painter->setPen(Qt::red);
            painter->drawRect(r);
#endif
        }
        break;
    default:
        break;
    }
    painter->restore();
}

static const qreal TURN = 10.;
static const qreal PW = 1.5;

/*! \return The bounding rectangle for the graphics used to represent
 * a constraint between points \a start and \a end (typically an
 * arrow)
 */
QRectF ItemDelegate::constraintBoundingRect(const QPointF &start, const QPointF &end) const
{
    QPolygonF poly;
    if (start.x() > end.x() - TURN) {
        if (start.y() < end.y()) {
            poly << QPointF(start.x() + TURN, start.y() - TURN / 2.)
                 << QPointF(end.x() - TURN, end.y() + TURN / 2.);
        } else {
            poly << QPointF(start.x() + TURN, start.y() + TURN / 2.)
                 << QPointF(end.x() - TURN, end.y() - TURN / 2.);
        }
    } else {
        if (start.y() < end.y()) {
            poly << QPointF(start.x(), start.y() - TURN / 2.)
                 << QPointF(end.x(), end.y() + TURN / 2.);
        } else {
            poly << QPointF(start.x(), start.y() + TURN / 2.)
                 << QPointF(end.x(), end.y() - TURN / 2.);
        }
    }
    return poly.boundingRect().adjusted(-PW, -PW, PW, PW);
}

/*! Paints the constraint item between points \a start and \a end
 * using \a painter and \a opt.
 *
 * \todo Review \a opt's type
 */
void ItemDelegate::paintConstraintItem(QPainter *painter, const QStyleOptionGraphicsItem &opt,
                                       const QPointF &start, const QPointF &end, const QPen &pen)
{
    Q_UNUSED(opt);
    qreal midx = end.x() - TURN;
    qreal midy = (end.y() - start.y()) / 2. + start.y();

    painter->setPen(pen);
    painter->setBrush(pen.color());

    if (start.x() > end.x() - TURN) {
        QPolygonF poly;
        poly << start
             << QPointF(start.x() + TURN, start.y())
             << QPointF(start.x() + TURN, midy)
             << QPointF(end.x() - TURN, midy)
             << QPointF(end.x() - TURN, end.y())
             << end;
        painter->drawPolyline(poly);
        QPolygonF arrow;
        arrow << end
              << QPointF(end.x() - TURN / 2., end.y() - TURN / 2.)
              << QPointF(end.x() - TURN / 2., end.y() + TURN / 2.);
        painter->drawPolygon(arrow);
    } else {
        QPolygonF poly;
        poly << start
             << QPointF(midx, start.y())
             << QPointF(midx, end.y())
             << end;
        painter->drawPolyline(poly);
        QPolygonF arrow;
        arrow << end
              << QPointF(end.x() - TURN / 2., end.y() - TURN / 2.)
              << QPointF(end.x() - TURN / 2., end.y() + TURN / 2.);
        painter->drawPolygon(arrow);
    }
}

