// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "networkreader.h"
#include "fileutil.h"

#include <QFileInfo>

NetworkReader::NetworkReader(const QString &procRoot, const QString &sysRoot)
    : m_procRoot(procRoot)
    , m_sysRoot(sysRoot)
{
}

void NetworkReader::sample(QVariantMap *metrics, double elapsed)
{
    QHash<QString, QPair<quint64, quint64>> networks;
    QVariantList interfaces;
    double download = 0;
    double upload = 0;
    quint64 received = 0;
    quint64 sent = 0;
    for (const QByteArray &line : readFile(m_procRoot + QStringLiteral("/net/dev")).split('\n')) {
        const int separator = line.indexOf(':');
        if (separator < 0)
            continue;
        const QString name = QString::fromLatin1(line.left(separator).trimmed());
        const auto values = splitFields(line.mid(separator + 1));
        if (name == QLatin1String("lo") || values.size() < 16)
            continue;
        const quint64 rx = values[0].toULongLong();
        const quint64 tx = values[8].toULongLong();
        networks.insert(name, { rx, tx });
        const auto previous = m_previous.value(name);
        const double rxRate = m_previous.contains(name) ? byteRate(rx, previous.first, elapsed) : 0;
        const double txRate = m_previous.contains(name) ? byteRate(tx, previous.second, elapsed) : 0;
        const bool physical = QFileInfo::exists(m_sysRoot + QStringLiteral("/class/net/") + name + QStringLiteral("/device"));
        if (physical) {
            download += rxRate;
            upload += txRate;
            received += rx;
            sent += tx;
        }
        interfaces.append(QVariantMap {
            { QStringLiteral("name"), name },
            { QStringLiteral("state"), QString::fromLatin1(readFile(m_sysRoot + QStringLiteral("/class/net/") + name + QStringLiteral("/operstate")).trimmed()) },
            { QStringLiteral("address"), QString::fromLatin1(readFile(m_sysRoot + QStringLiteral("/class/net/") + name + QStringLiteral("/address")).trimmed()) },
            { QStringLiteral("download"), rxRate },
            { QStringLiteral("upload"), txRate },
            { QStringLiteral("received"), rx },
            { QStringLiteral("sent"), tx },
            { QStringLiteral("physical"), physical }
        });
    }
    metrics->insert(QStringLiteral("download"), download);
    metrics->insert(QStringLiteral("upload"), upload);
    metrics->insert(QStringLiteral("received"), received);
    metrics->insert(QStringLiteral("sent"), sent);
    metrics->insert(QStringLiteral("interfaces"), interfaces);

    m_previous = networks;
}
