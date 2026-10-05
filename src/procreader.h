// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QString>
#include <QVariantMap>
#include <QVector>

struct CpuCounters
{
    quint64 total = 0;
    quint64 idle = 0;
};

struct ProcessRecord
{
    int pid = 0;
    int parentPid = 0;
    uint uid = 0;
    QString name;
    QString displayName;
    QString iconName;
    QString user;
    QString command;
    QString executable;
    QString program;
    QString state;
    int threads = 0;
    int nice = 0;
    quint64 startTicks = 0;
    quint64 cpuTicks = 0;
    quint64 memory = 0;
    quint64 readBytes = 0;
    quint64 writeBytes = 0;
    bool ioAvailable = false;
    bool kernelThread = false;
    double cpu = 0;
    double readRate = 0;
    double writeRate = 0;
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
    static QVariantList volumes();
    static CpuCounters parseCpu(const QByteArray &line);
    static QVariantMap parseMemory(const QByteArray &contents);
    static bool parseProcess(const QByteArray &contents, ProcessRecord *process,
                             qint64 pageSize);
    static bool parseUid(const QByteArray &status, uint *uid);
    static QString applicationProgram(const QByteArray &commandLine);
    static double cpuUsage(const CpuCounters &current, const CpuCounters &previous);
    static QString stateName(const QString &state);

private:
    QByteArray read(const QString &path) const;
    void readIdentity(const QString &path, ProcessRecord *process) const;
    QString userName(uint uid);

    QString m_procRoot;
    QString m_sysRoot;
    QElapsedTimer m_clock;
    qint64 m_previousTime = 0;
    bool m_hasPreviousSample = false;
    qint64 m_pageSize = 4096;
    QHash<QString, CpuCounters> m_previousCpu;
    QHash<int, ProcessRecord> m_previousProcesses;
    QHash<QString, QPair<quint64, quint64>> m_previousNetworks;
    QHash<QString, QPair<quint64, quint64>> m_previousDisks;
    QHash<uint, QString> m_users;
    QString m_cpuName;
};
