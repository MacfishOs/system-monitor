// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QPair>
#include <QString>
#include <QVariantMap>

class NetworkReader
{
public:
    NetworkReader(const QString &procRoot, const QString &sysRoot);

    void sample(QVariantMap *metrics, double elapsed);

private:
    QString m_procRoot;
    QString m_sysRoot;
    QHash<QString, QPair<quint64, quint64>> m_previous;
};
