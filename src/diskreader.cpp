// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "diskreader.h"
#include "fileutil.h"

#include <QFileInfo>
#include <QStorageInfo>

DiskReader::DiskReader(const QString &procRoot, const QString &sysRoot)
    : m_procRoot(procRoot)
    , m_sysRoot(sysRoot)
{
}

void DiskReader::sample(QVariantMap *metrics, double elapsed)
{
    QHash<QString, QPair<quint64, quint64>> disks;
    QVariantList diskDevices;
    double diskRead = 0;
    double diskWrite = 0;
    for (const QByteArray &line : readFile(m_procRoot + QStringLiteral("/diskstats")).split('\n')) {
        const auto values = splitFields(line);
        if (values.size() < 14)
            continue;
        const QString name = QString::fromLatin1(values[2]);
        const QString path = m_sysRoot + QStringLiteral("/block/") + name;
        if (!QFileInfo::exists(path + QStringLiteral("/device")))
            continue;
        const quint64 readBytes = values[5].toULongLong() * 512;
        const quint64 writeBytes = values[9].toULongLong() * 512;
        disks.insert(name, { readBytes, writeBytes });
        const auto previous = m_previous.value(name);
        const double readRate = m_previous.contains(name) ? byteRate(readBytes, previous.first, elapsed) : 0;
        const double writeRate = m_previous.contains(name) ? byteRate(writeBytes, previous.second, elapsed) : 0;
        diskRead += readRate;
        diskWrite += writeRate;
        diskDevices.append(QVariantMap {
            { QStringLiteral("name"), name },
            { QStringLiteral("model"), QString::fromUtf8(readFile(path + QStringLiteral("/device/model")).trimmed()) },
            { QStringLiteral("read"), readRate },
            { QStringLiteral("write"), writeRate },
            { QStringLiteral("size"), readFile(path + QStringLiteral("/size")).trimmed().toULongLong() * 512 }
        });
    }
    metrics->insert(QStringLiteral("diskRead"), diskRead);
    metrics->insert(QStringLiteral("diskWrite"), diskWrite);
    metrics->insert(QStringLiteral("disks"), diskDevices);
    m_previous = disks;
}

QVariantList DiskReader::volumes()
{
    QVariantList volumes;
    for (const QStorageInfo &volume : QStorageInfo::mountedVolumes()) {
        if (!volume.isValid() || !volume.isReady() || volume.bytesTotal() <= 0)
            continue;
        const QByteArray type = volume.fileSystemType();
        if (type == "tmpfs" || type == "devtmpfs" || type == "squashfs"
                || volume.rootPath().startsWith(QLatin1String("/proc"))
                || volume.rootPath().startsWith(QLatin1String("/sys")))
            continue;
        volumes.append(QVariantMap {
            { QStringLiteral("mount"), volume.rootPath() },
            { QStringLiteral("device"), QString::fromLocal8Bit(volume.device()) },
            { QStringLiteral("type"), QString::fromLatin1(type) },
            { QStringLiteral("total"), volume.bytesTotal() },
            { QStringLiteral("available"), volume.bytesAvailable() },
            { QStringLiteral("used"), volume.bytesTotal() - volume.bytesFree() }
        });
    }
    return volumes;
}
