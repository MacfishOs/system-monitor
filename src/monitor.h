// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "processmodel.h"

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QObject>
#include <QTimer>
#include <memory>

class DesktopEntry;

class Monitor : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.cutefish.SystemMonitor")
    Q_PROPERTY(QVariantMap metrics READ metrics NOTIFY updated)
    Q_PROPERTY(QVariantMap history READ history NOTIFY updated)
    Q_PROPERTY(ProcessFilter *processes READ processes CONSTANT)
    Q_PROPERTY(ProcessFilter *applications READ applications CONSTANT)
    Q_PROPERTY(int scope READ scope WRITE setScope NOTIFY scopeChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY updated)
    Q_PROPERTY(QString error READ error NOTIFY updated)
    Q_PROPERTY(int interval READ interval WRITE setInterval NOTIFY intervalChanged)
    Q_PROPERTY(int historyDuration READ historyDuration CONSTANT)

public:
    explicit Monitor(QObject *parent = nullptr);
    ~Monitor() override;

    QVariantMap metrics() const;
    QVariantMap history() const;
    ProcessFilter *processes();
    ProcessFilter *applications();
    // Index of the process list's segment: applications, own processes, all processes.
    int scope() const;
    void setScope(int scope);
    bool ready() const;
    QString error() const;
    int interval() const;
    void setInterval(int interval);
    int historyDuration() const;

    Q_INVOKABLE QVariantMap process(int pid, const QString &startTicks) const;
    Q_INVOKABLE QString endProcess(int pid, const QString &startTicks, bool force);
    Q_INVOKABLE QVariantMap application(int pid, const QString &startTicks) const;
    Q_INVOKABLE QString endApplication(int pid, const QString &startTicks, bool force);
    Q_INVOKABLE void sortBy(const QString &column);
    Q_INVOKABLE QString formatBytes(double bytes) const;
    Q_INVOKABLE QString formatRate(double bytes) const;
    Q_INVOKABLE QString formatPercent(double percent) const;
    Q_INVOKABLE QString formatTemperature(double celsius) const;
    Q_INVOKABLE void copyText(const QString &text);
    static QString signalProcess(int pid, quint64 startTicks, bool force);

public slots:
    void activateWindow();

signals:
    void updated();
    void intervalChanged();
    void scopeChanged();
    void activationRequested();
    void copyRequested(const QString &text);

private:
    void savePreferences() const;
    void sample();
    void acceptSample();
    void identify(ProcessRecord *process);
    QVector<ProcessRecord> groupApplications(const QVector<ProcessRecord> &processes);
    static bool canManage(const ProcessRecord &process);

    struct Application
    {
        QString id;
        QString name;
        QString icon;
    };

    static Application describeEntry(DesktopEntry *entry);

    ProcessModel m_model;
    ProcessFilter m_filter;
    ProcessModel m_applicationModel;
    ProcessFilter m_applicationFilter;
    int m_scope = 1;
    QHash<QString, Application> m_applicationIds;
    std::shared_ptr<ProcReader> m_reader;
    QFutureWatcher<Snapshot> m_watcher;
    QFutureWatcher<QVariantList> m_volumeWatcher;
    QVariantList m_volumes;
    QHash<QString, Application> m_applications;
    QTimer m_timer;
    QVariantMap m_metrics;
    QHash<QString, QVariantList> m_history;
    QVariantList m_historyTimes;
    QElapsedTimer m_historyClock;
    QString m_error;
    bool m_ready = false;
};
