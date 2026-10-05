// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fileutil.h"

#include <QFile>

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

QList<QByteArray> splitFields(const QByteArray &line)
{
    return line.simplified().split(' ');
}

quint64 counterDelta(quint64 current, quint64 previous)
{
    return current >= previous ? current - previous : 0;
}

double byteRate(quint64 current, quint64 previous, double elapsed)
{
    return elapsed > 0 ? counterDelta(current, previous) / elapsed : 0;
}
