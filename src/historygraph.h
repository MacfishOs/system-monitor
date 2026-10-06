// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QQuickPaintedItem>

class HistoryGraph : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QVariantList values READ values WRITE setValues NOTIFY valuesChanged)
    Q_PROPERTY(QVariantList secondaryValues READ secondaryValues WRITE setSecondaryValues NOTIFY valuesChanged)
    Q_PROPERTY(QVariantList timestamps READ timestamps WRITE setTimestamps NOTIFY valuesChanged)
    Q_PROPERTY(qint64 duration READ duration WRITE setDuration NOTIFY appearanceChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor secondaryColor READ secondaryColor WRITE setSecondaryColor NOTIFY appearanceChanged)
    Q_PROPERTY(QColor gridColor READ gridColor WRITE setGridColor NOTIFY appearanceChanged)
    Q_PROPERTY(double maximum READ maximum WRITE setMaximum NOTIFY appearanceChanged)
    Q_PROPERTY(double scaleFloor READ scaleFloor WRITE setScaleFloor NOTIFY appearanceChanged)
    Q_PROPERTY(bool curved READ curved WRITE setCurved NOTIFY appearanceChanged)
    Q_PROPERTY(double lineWidth READ lineWidth WRITE setLineWidth NOTIFY appearanceChanged)
    Q_PROPERTY(double effectiveMaximum READ effectiveMaximum NOTIFY valuesChanged)

public:
    explicit HistoryGraph(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;
    QVariantList values() const;
    void setValues(const QVariantList &values);
    QVariantList secondaryValues() const;
    void setSecondaryValues(const QVariantList &values);
    QVariantList timestamps() const;
    void setTimestamps(const QVariantList &timestamps);
    qint64 duration() const;
    void setDuration(qint64 duration);
    QColor color() const;
    void setColor(const QColor &color);
    QColor secondaryColor() const;
    void setSecondaryColor(const QColor &color);
    QColor gridColor() const;
    void setGridColor(const QColor &color);
    double maximum() const;
    void setMaximum(double maximum);
    // The smallest full scale an automatic maximum may pick.
    double scaleFloor() const;
    void setScaleFloor(double floor);
    bool curved() const;
    void setCurved(bool curved);
    double lineWidth() const;
    void setLineWidth(double width);
    double effectiveMaximum() const;

signals:
    void valuesChanged();
    void appearanceChanged();

private:
    QVariantList m_values;
    QVariantList m_secondaryValues;
    QVariantList m_timestamps;
    qint64 m_duration = 60000;
    QColor m_color;
    QColor m_secondaryColor;
    QColor m_gridColor;
    double m_maximum = 100;
    double m_scaleFloor = 1024;
    bool m_curved = false;
    double m_lineWidth = 1.5;
};
