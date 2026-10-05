// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "processrecord.h"

#include <QHash>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

// One engine of a DRM client's fdinfo: busy time in nanoseconds, or busy
// and total GPU cycles where the driver counts those instead.
struct DrmEngine
{
    quint64 busyTime = 0;
    quint64 cycles = 0;
    quint64 totalCycles = 0;
    int capacity = 1;
    bool hasTime = false;
    bool hasCycles = false;
};

struct DrmClient
{
    QByteArray id;
    QHash<QByteArray, DrmEngine> engines;
};

class GpuReader
{
public:
    GpuReader(const QString &procRoot, const QString &sysRoot);

    void sample(QVariantMap *metrics, const QVector<ProcessRecord> &processes, double elapsed);
    static DrmClient parseDrmFdinfo(const QByteArray &contents);
    static double drmBusy(const QHash<QByteArray, DrmClient> &current,
                          const QHash<QByteArray, DrmClient> &previous, double elapsedTime);

private:
    struct Gpu
    {
        QString device;
        QString hwmon;
        QString name;
        QString driver;
        QStringList nodes;
        QHash<QByteArray, DrmClient> clients;
        bool primary = false;
        bool hasBusy = false;
        bool hasClientBusy = false;
        bool hasTemperature = false;
        bool hasFrequency = false;
        bool hasVram = false;
    };

    // The DRM descriptors a process held when they were last listed.
    struct Descriptors
    {
        quint64 startTicks = 0;
        QVector<QPair<QByteArray, int>> nodes;
    };

    void scan();
    static QString pciName(const QByteArray &vendor, const QByteArray &device);
    QVector<QPair<QByteArray, int>> drmDescriptors(const QString &path) const;
    QVector<QHash<QByteArray, DrmClient>> drmClients(const QVector<ProcessRecord> &processes,
                                                     const QVector<bool> &wanted);

    QString m_procRoot;
    QString m_sysRoot;
    uint m_uid = 0;
    QVector<Gpu> m_gpus;
    QHash<QByteArray, int> m_nodeOwners;
    QHash<int, Descriptors> m_descriptors;
    quint64 m_samples = 0;
    bool m_scanned = false;
};
