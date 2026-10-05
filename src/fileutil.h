// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

QByteArray readFile(const QString &path);
QList<QByteArray> splitFields(const QByteArray &line);
quint64 counterDelta(quint64 current, quint64 previous);
double byteRate(quint64 current, quint64 previous, double elapsed);
