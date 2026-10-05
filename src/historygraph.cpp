// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "historygraph.h"

#include <QPainter>
#include <QPainterPath>
#include <cmath>

HistoryGraph::HistoryGraph(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
}

QVariantList HistoryGraph::values() const { return m_values; }
QVariantList HistoryGraph::secondaryValues() const { return m_secondaryValues; }
QVariantList HistoryGraph::timestamps() const { return m_timestamps; }
qint64 HistoryGraph::duration() const { return m_duration; }
QColor HistoryGraph::color() const { return m_color; }
QColor HistoryGraph::secondaryColor() const { return m_secondaryColor; }
QColor HistoryGraph::gridColor() const { return m_gridColor; }
double HistoryGraph::maximum() const { return m_maximum; }

void HistoryGraph::setValues(const QVariantList &values)
{
    m_values = values;
    update();
    emit valuesChanged();
}

void HistoryGraph::setSecondaryValues(const QVariantList &values)
{
    m_secondaryValues = values;
    update();
    emit valuesChanged();
}

void HistoryGraph::setTimestamps(const QVariantList &timestamps)
{
    m_timestamps = timestamps;
    update();
    emit valuesChanged();
}

void HistoryGraph::setDuration(qint64 duration)
{
    if (duration <= 0 || duration == m_duration)
        return;
    m_duration = duration;
    update();
    emit appearanceChanged();
}

void HistoryGraph::setColor(const QColor &color)
{
    m_color = color;
    update();
    emit appearanceChanged();
}

void HistoryGraph::setSecondaryColor(const QColor &color)
{
    m_secondaryColor = color;
    update();
    emit appearanceChanged();
}

void HistoryGraph::setGridColor(const QColor &color)
{
    m_gridColor = color;
    update();
    emit appearanceChanged();
}

void HistoryGraph::setMaximum(double maximum)
{
    m_maximum = maximum;
    update();
    emit appearanceChanged();
    emit valuesChanged();
}

double HistoryGraph::effectiveMaximum() const
{
    if (m_maximum > 0)
        return m_maximum;
    double peak = 1024;
    for (const auto &series : { m_values, m_secondaryValues }) {
        for (const QVariant &value : series)
            peak = qMax(peak, value.toDouble());
    }
    return std::pow(2.0, std::ceil(std::log2(peak)));
}

void HistoryGraph::paint(QPainter *painter)
{
    const QRectF area(1, 2, qMax(0.0, width() - 2), qMax(0.0, height() - 4));
    if (area.isEmpty())
        return;
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(m_gridColor, 1));
    for (int i = 0; i <= 4; ++i) {
        const double y = area.top() + area.height() * i / 4;
        painter->drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
    }

    const double scale = effectiveMaximum();
    painter->save();
    painter->setClipRect(boundingRect());
    const auto draw = [&](const QVariantList &values, const QColor &color, bool filled) {
        if (values.isEmpty())
            return;
        QPainterPath line;
        const bool timed = m_timestamps.size() == values.size();
        for (int i = 0; i < values.size(); ++i) {
            const qint64 age = timed ? m_timestamps.last().toLongLong() - m_timestamps[i].toLongLong()
                                     : (values.size() - 1 - i) * 1000;
            const double x = area.right() - area.width() * age / m_duration;
            const double y = area.bottom() - area.height() * qBound(0.0, values[i].toDouble() / scale, 1.0);
            if (i == 0)
                line.moveTo(x, y);
            else
                line.lineTo(x, y);
        }
        if (filled && values.size() > 1) {
            QPainterPath fill = line;
            fill.lineTo(area.right(), area.bottom());
            fill.lineTo(line.pointAtPercent(0).x(), area.bottom());
            fill.closeSubpath();
            QLinearGradient gradient(area.topLeft(), area.bottomLeft());
            QColor top = color;
            top.setAlphaF(0.18);
            QColor bottom = color;
            bottom.setAlphaF(0.02);
            gradient.setColorAt(0, top);
            gradient.setColorAt(1, bottom);
            painter->fillPath(fill, gradient);
        }
        painter->setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        if (values.size() == 1)
            painter->drawPoint(line.currentPosition());
        else
            painter->drawPath(line);
    };
    draw(m_values, m_color, true);
    draw(m_secondaryValues, m_secondaryColor, false);
    painter->restore();
}
