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
double HistoryGraph::scaleFloor() const { return m_scaleFloor; }
bool HistoryGraph::curved() const { return m_curved; }
double HistoryGraph::lineWidth() const { return m_lineWidth; }

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

void HistoryGraph::setScaleFloor(double floor)
{
    m_scaleFloor = floor;
    update();
    emit appearanceChanged();
    emit valuesChanged();
}

void HistoryGraph::setCurved(bool curved)
{
    m_curved = curved;
    update();
    emit appearanceChanged();
}

void HistoryGraph::setLineWidth(double width)
{
    m_lineWidth = width;
    update();
    emit appearanceChanged();
}

double HistoryGraph::effectiveMaximum() const
{
    if (m_maximum > 0)
        return m_maximum;
    double peak = m_scaleFloor;
    for (const auto &series : { m_values, m_secondaryValues }) {
        for (const QVariant &value : series)
            peak = qMax(peak, value.toDouble());
    }
    return std::pow(2.0, std::ceil(std::log2(peak)));
}

static QPainterPath linearPath(const QList<QPointF> &points)
{
    QPainterPath path(points.first());
    for (int i = 1; i < points.size(); ++i)
        path.lineTo(points[i]);
    return path;
}

// Monotone cubic interpolation: the curve passes through every sample and never overshoots
// between them, so it shows no peak or dip that was not measured.
static QPainterPath monotonePath(const QList<QPointF> &points)
{
    const int count = points.size();
    if (count < 3)
        return linearPath(points);

    QList<double> slopes(count - 1);
    for (int i = 0; i < count - 1; ++i) {
        const double dx = points[i + 1].x() - points[i].x();
        slopes[i] = dx > 0 ? (points[i + 1].y() - points[i].y()) / dx : 0;
    }

    QList<double> tangents(count);
    tangents[0] = slopes[0];
    tangents[count - 1] = slopes[count - 2];
    for (int i = 1; i < count - 1; ++i) {
        const double before = slopes[i - 1];
        const double after = slopes[i];
        if (before * after <= 0) {
            tangents[i] = 0;
            continue;
        }
        const double h0 = points[i].x() - points[i - 1].x();
        const double h1 = points[i + 1].x() - points[i].x();
        const double average = (before * h1 + after * h0) / (h0 + h1);
        const double sign = before > 0 ? 1 : -1;
        tangents[i] = sign * qMin(qMin(std::abs(before), std::abs(after)) * 2, std::abs(average));
    }

    QPainterPath path(points.first());
    for (int i = 0; i < count - 1; ++i) {
        const double third = (points[i + 1].x() - points[i].x()) / 3;
        path.cubicTo(QPointF(points[i].x() + third, points[i].y() + tangents[i] * third),
                     QPointF(points[i + 1].x() - third, points[i + 1].y() - tangents[i + 1] * third),
                     points[i + 1]);
    }
    return path;
}

void HistoryGraph::paint(QPainter *painter)
{
    const QRectF area(1, 2, qMax(0.0, width() - 2), qMax(0.0, height() - 4));
    if (area.isEmpty())
        return;
    painter->setRenderHint(QPainter::Antialiasing);
    if (m_gridColor.isValid()) {
        painter->setPen(QPen(m_gridColor, 1));
        for (int i = 0; i <= 4; ++i) {
            const double y = area.top() + area.height() * i / 4;
            painter->drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
        }
    }

    const double scale = effectiveMaximum();
    painter->save();
    painter->setClipRect(boundingRect());
    const auto draw = [&](const QVariantList &values, const QColor &color, bool filled) {
        if (values.isEmpty() && !filled)
            return;

        // History before the first sample is drawn as idle: flat along the bottom, rising to the
        // first sample over one sampling interval. An empty primary series is just that baseline.
        QList<QPointF> points;
        const bool timed = m_timestamps.size() == values.size();
        for (int i = 0; i < values.size(); ++i) {
            const qint64 age = timed ? m_timestamps.last().toLongLong() - m_timestamps[i].toLongLong()
                                     : (values.size() - 1 - i) * 1000;
            const double x = area.right() - area.width() * age / m_duration;
            const double y = area.bottom() - area.height() * qBound(0.0, values[i].toDouble() / scale, 1.0);
            points.append(QPointF(x, y));
        }
        if (points.isEmpty()) {
            points = { QPointF(area.left(), area.bottom()), QPointF(area.right(), area.bottom()) };
        } else if (points.first().x() > area.left()) {
            const double step = points.size() > 1 ? points[1].x() - points[0].x() : area.width() * 1000 / m_duration;
            const double rampStart = qMax(area.left(), points.first().x() - step);
            if (rampStart > area.left())
                points.prepend(QPointF(rampStart, area.bottom()));
            points.prepend(QPointF(area.left(), area.bottom()));
        }

        QPainterPath line = m_curved ? monotonePath(points) : linearPath(points);
        if (filled) {
            QPainterPath fill = line;
            fill.lineTo(points.last().x(), area.bottom());
            fill.lineTo(points.first().x(), area.bottom());
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
        painter->setPen(QPen(color, m_lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->drawPath(line);
    };
    draw(m_values, m_color, true);
    draw(m_secondaryValues, m_secondaryColor, false);
    painter->restore();
}
