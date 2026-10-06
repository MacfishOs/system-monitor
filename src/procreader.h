// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "diskreader.h"
#include "gpureader.h"
#include "networkreader.h"
#include "processrecord.h"

#include <QElapsedTimer>
#include <QHash>
#include <QSet>
#include <QString>
#include <QVariantMap>
#include <QVector>

struct CpuCounters
{
    quint64 total = 0;
    quint64 idle = 0;
};

struct Snapshot
{
    bool available = false;
    bool baseline = false;
    QString error;
    QVariantMap metrics;
    QVector<ProcessRecord> processes;
};

class ProcReader
{
public:
    explicit ProcReader(const QString &procRoot = QStringLiteral("/proc"),
                        const QString &sysRoot = QStringLiteral("/sys"));

    Snapshot sample();
    static CpuCounters parseCpu(const QByteArray &line);
    static QVariantMap parseMemory(const QByteArray &contents);
    static bool parseProcess(const QByteArray &contents, ProcessRecord *process,
                             qint64 pageSize);
    static bool parseUid(const QByteArray &status, uint *uid);
    static QString applicationProgram(const QByteArray &commandLine);
    static QString scopeApplication(const QByteArray &cgroup);
    static bool parsePss(const QByteArray &rollup, quint64 *pss);
    static double cpuUsage(const CpuCounters &current, const CpuCounters &previous);
    static QString stateName(const QString &state);

private:
    void readIdentity(const QString &path, ProcessRecord *process) const;
    QString userName(uint uid);

    QString m_procRoot;
    QElapsedTimer m_clock;
    qint64 m_previousTime = 0;
    bool m_hasPreviousSample = false;
    qint64 m_pageSize = 4096;
    uint m_uid = 0;
    QHash<QString, CpuCounters> m_previousCpu;
    QHash<int, ProcessRecord> m_previousProcesses;
    QHash<uint, QString> m_users;
    QString m_cpuName;
    NetworkReader m_networks;
    DiskReader m_disks;
    GpuReader m_gpus;
};
