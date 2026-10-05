// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QSet>
#include <QString>
#include <QVariantMap>
#include <QVector>

#include <vector>

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
    QString scopeId;
    QString appId;
    QString state;
    int threads = 0;
    int nice = 0;
    quint64 startTicks = 0;
    quint64 cpuTicks = 0;
    quint64 memory = 0;
    quint64 readBytes = 0;
    quint64 writeBytes = 0;
    quint64 pss = 0;
    qint64 pssTime = 0;
    bool pssAvailable = false;
    bool ioAvailable = false;
    bool kernelThread = false;
    bool scopeSettled = false;
    double cpu = 0;
    double readRate = 0;
    double writeRate = 0;
    // Set on application groups only: every process the application owns.
    std::vector<ProcessRecord> members;
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

    // PSS is read for the processes of applicationPids, their new children
    // and processes in application cgroups: it is too costly for all of them.
    Snapshot sample(const QSet<int> &applicationPids = {});
    static QVariantList volumes();
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
    struct Gpu
    {
        QString device;
        QString hwmon;
        QString name;
        QString driver;
        bool primary = false;
        bool hasBusy = false;
        bool hasTemperature = false;
        bool hasFrequency = false;
        bool hasVram = false;
    };

    QByteArray read(const QString &path) const;
    void readIdentity(const QString &path, ProcessRecord *process) const;
    QString userName(uint uid);
    void scanGpus();
    QString pciName(const QByteArray &vendor, const QByteArray &device) const;
    void sampleGpus(QVariantMap *metrics);

    QString m_procRoot;
    QString m_sysRoot;
    QElapsedTimer m_clock;
    qint64 m_previousTime = 0;
    bool m_hasPreviousSample = false;
    qint64 m_pageSize = 4096;
    uint m_uid = 0;
    QHash<QString, CpuCounters> m_previousCpu;
    QHash<int, ProcessRecord> m_previousProcesses;
    QHash<QString, QPair<quint64, quint64>> m_previousNetworks;
    QHash<QString, QPair<quint64, quint64>> m_previousDisks;
    QHash<uint, QString> m_users;
    QString m_cpuName;
    QVector<Gpu> m_gpus;
    bool m_gpusScanned = false;
};
