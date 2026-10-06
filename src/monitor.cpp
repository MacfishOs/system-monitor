// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "monitor.h"

#include "applicationregistry.h"
#include "desktopentry.h"
#include "preferences.h"

#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QSettings>
#include <QtConcurrent>

#include <algorithm>

#include <cerrno>
#include <cstring>
#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>

Monitor::Monitor(QObject *parent)
    : QObject(parent)
    , m_model(this)
    , m_filter(&m_model, this)
    , m_applicationModel(this)
    , m_applicationFilter(&m_applicationModel, this)
    , m_reader(std::make_shared<ProcReader>())
{
    m_metrics = ProcReader::parseMemory({});
    for (const QString &key : { QStringLiteral("cpuPercent"), QStringLiteral("download"),
                              QStringLiteral("upload"), QStringLiteral("diskRead"),
                              QStringLiteral("diskWrite"),
                              QStringLiteral("received"), QStringLiteral("sent"),
                              QStringLiteral("processCount"), QStringLiteral("coreCount"),
                              QStringLiteral("gpuPercent"), QStringLiteral("gpuTemperature") })
        m_metrics.insert(key, 0);
    for (const QString &key : { QStringLiteral("cores"), QStringLiteral("interfaces"),
                              QStringLiteral("disks"), QStringLiteral("volumes"), QStringLiteral("gpus") })
        m_metrics.insert(key, QVariantList());
    for (const QString &key : { QStringLiteral("gpuAvailable"), QStringLiteral("gpuHasBusy"),
                              QStringLiteral("gpuHasTemperature") })
        m_metrics.insert(key, false);
    m_metrics.insert(QStringLiteral("cpuName"), QString());
    m_metrics.insert(QStringLiteral("gpuName"), QString());
    QSettings settings(Preferences::ownFile(), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Processes"));
    m_scope = qBound(0, settings.value(QStringLiteral("Scope"), m_scope).toInt(), 2);
    m_filter.setAllUsers(m_scope == 2);
    const QString column = settings.value(QStringLiteral("SortColumn"), m_filter.sortColumn()).toString();
    const bool ascending = settings.value(QStringLiteral("SortAscending"), m_filter.ascending()).toBool();
    m_filter.setSorting(column, ascending);
    m_applicationFilter.setSorting(column, ascending);

    m_historyClock.start();
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &Monitor::sample);
    connect(&m_watcher, &QFutureWatcher<Snapshot>::finished, this, &Monitor::acceptSample);
    connect(&m_volumeWatcher, &QFutureWatcher<QVariantList>::finished, this, [this] {
        m_volumes = m_volumeWatcher.result();
    });
    connect(ApplicationRegistry::instance(), &ApplicationRegistry::applicationsChanged, this, [this] {
        m_applications.clear();
        m_applicationIds.clear();
    });
    m_timer.start();
    sample();
}

Monitor::~Monitor()
{
    m_watcher.waitForFinished();
}

QVariantMap Monitor::metrics() const { return m_metrics; }
ProcessFilter *Monitor::processes() { return &m_filter; }
ProcessFilter *Monitor::applications() { return &m_applicationFilter; }
int Monitor::scope() const { return m_scope; }
bool Monitor::ready() const { return m_ready; }
QString Monitor::error() const { return m_error; }
int Monitor::interval() const { return m_timer.interval(); }
int Monitor::historyDuration() const { return 60; }

QVariantMap Monitor::history() const
{
    QVariantMap result;
    for (auto it = m_history.cbegin(); it != m_history.cend(); ++it)
        result.insert(it.key(), it.value());
    result.insert(QStringLiteral("timestamps"), m_historyTimes);
    return result;
}

void Monitor::setInterval(int interval)
{
    if ((interval != 1000 && interval != 2000 && interval != 5000) || interval == m_timer.interval())
        return;
    m_timer.setInterval(interval);
    emit intervalChanged();
    emit updated();
}

void Monitor::sample()
{
    // A hung network mount blocks statfs; it must not stall the other measurements.
    if (!m_volumeWatcher.isRunning())
        m_volumeWatcher.setFuture(QtConcurrent::run(&DiskReader::volumes));

    if (m_watcher.isRunning())
        return;
    const auto reader = m_reader;
    const QSet<int> applicationPids = m_applicationPids;
    m_watcher.setFuture(QtConcurrent::run([reader, applicationPids] {
        return reader->sample(applicationPids);
    }));
}

void Monitor::acceptSample()
{
    Snapshot snapshot = m_watcher.result();
    if (snapshot.baseline) {
        // Rates need a previous sample; publish from the next one.
        QTimer::singleShot(250, this, &Monitor::sample);
        return;
    }

    m_error = snapshot.error;
    if (snapshot.available) {
        m_ready = true;
        m_metrics = snapshot.metrics;
        m_metrics.insert(QStringLiteral("volumes"), m_volumes);
        for (ProcessRecord &process : snapshot.processes)
            identify(&process);
        m_model.apply(snapshot.processes);
        m_applicationModel.apply(groupApplications(snapshot.processes));
        const qint64 timestamp = m_historyClock.elapsed();
        m_historyTimes.append(timestamp);
        for (const QString &key : { QStringLiteral("cpuPercent"), QStringLiteral("memoryPercent"),
                                  QStringLiteral("download"), QStringLiteral("upload"),
                                  QStringLiteral("diskRead"), QStringLiteral("diskWrite"),
                                  QStringLiteral("gpuPercent"), QStringLiteral("gpuTemperature") }) {
            auto &values = m_history[key];
            values.append(m_metrics.value(key));
        }

        // Retain the sample before the visible window to interpolate its left edge.
        const qint64 cutoff = timestamp - historyDuration() * 1000;
        while (m_historyTimes.size() > 2 && m_historyTimes[1].toLongLong() < cutoff) {
            m_historyTimes.removeFirst();
            for (auto &values : m_history)
                values.removeFirst();
        }
    }
    emit updated();
}

Monitor::Application Monitor::describeEntry(DesktopEntry *entry)
{
    if (!entry)
        return {};
    // Hidden entries (Xwayland, handlers) name a process but are no application.
    const bool listed = !entry->noDisplay() && !entry->hidden();
    return { listed ? entry->id() : QString(), entry->name(), entry->icon() };
}

void Monitor::identify(ProcessRecord *process)
{
    if (process->program.isEmpty())
        return;

    auto application = m_applications.constFind(process->program);
    if (application == m_applications.cend()) {
        // A process has no app id; its own name stands in for one, so the
        // application's entry outranks web apps or shortcuts launching it.
        const QString name = QFileInfo(process->program).fileName();
        DesktopEntry *entry = ApplicationRegistry::instance()->resolveWindow(name, QString(),
                                                                             process->program, name);
        application = m_applications.insert(process->program,
                                            describeEntry(entry));
    }
    process->appId = application->id;
    if (!application->name.isEmpty())
        process->displayName = application->name;
    if (!application->icon.isEmpty())
        process->iconName = application->icon;
}

QVector<ProcessRecord> Monitor::groupApplications(const QVector<ProcessRecord> &processes)
{
    const uint uid = getuid();
    QHash<int, int> rows;
    for (int i = 0; i < processes.size(); ++i)
        rows.insert(processes[i].pid, i);

    // A process with a desktop entry of its own is that application; one
    // without joins its nearest such ancestor, then the application cgroup
    // a launcher put it in. A browser started from a terminal stays apart.
    QHash<QString, QVector<int>> groups;
    QStringList order;
    for (int i = 0; i < processes.size(); ++i) {
        const ProcessRecord &process = processes[i];
        if (process.uid != uid || process.kernelThread)
            continue;

        QString id = process.appId;
        int ancestor = rows.value(process.parentPid, -1);
        for (int depth = 0; id.isEmpty() && ancestor >= 0 && depth < 64; ++depth) {
            const ProcessRecord &parent = processes[ancestor];
            if (parent.uid != uid || parent.pid <= 1)
                break;
            id = parent.appId;
            ancestor = rows.value(parent.parentPid, -1);
        }
        if (id.isEmpty() && !process.scopeId.isEmpty()) {
            auto scope = m_applicationIds.constFind(process.scopeId);
            if (scope == m_applicationIds.cend()) {
                DesktopEntry *entry = ApplicationRegistry::instance()->byId(process.scopeId);
                scope = m_applicationIds.insert(process.scopeId, describeEntry(entry));
            }
            id = scope->id;
        }
        if (id.isEmpty())
            continue;
        if (!groups.contains(id))
            order.append(id);
        groups[id].append(i);
    }

    QVector<ProcessRecord> applications;
    m_applicationPids.clear();
    for (const QString &id : std::as_const(order)) {
        const QVector<int> &indexes = groups[id];
        QSet<int> pids;
        for (const int index : indexes)
            pids.insert(processes[index].pid);

        // The oldest process the application started itself stands for it.
        const ProcessRecord *main = nullptr;
        for (const int index : indexes) {
            const ProcessRecord &process = processes[index];
            if (pids.contains(process.parentPid))
                continue;
            const bool own = process.appId == id;
            const bool mainOwn = main && main->appId == id;
            if (!main || (own && !mainOwn) || (own == mainOwn && process.startTicks < main->startTicks))
                main = &process;
        }

        auto application = m_applicationIds.constFind(id);
        if (application == m_applicationIds.cend()) {
            DesktopEntry *entry = ApplicationRegistry::instance()->byId(id);
            application = m_applicationIds.insert(id, entry ? Application { entry->id(), entry->name(), entry->icon() }
                                                            : Application { id, main->displayName, main->iconName });
        }

        ProcessRecord group = *main;
        group.appId = id;
        group.displayName = application->name.isEmpty() ? main->displayName : application->name;
        group.iconName = application->icon.isEmpty() ? main->iconName : application->icon;
        group.cpu = 0;
        group.memory = 0;
        group.readRate = 0;
        group.writeRate = 0;
        group.threads = 0;
        group.ioAvailable = false;
        group.pssAvailable = true;
        for (const int index : indexes) {
            const ProcessRecord &process = processes[index];
            group.cpu += process.cpu;
            group.memory += process.pssAvailable ? process.pss : process.memory;
            group.pssAvailable = group.pssAvailable && process.pssAvailable;
            group.threads += process.threads;
            if (process.ioAvailable) {
                group.ioAvailable = true;
                group.readRate += process.readRate;
                group.writeRate += process.writeRate;
            }
            group.members.push_back(process);
            m_applicationPids.insert(process.pid);
        }
        group.cpu = qMin(100.0, group.cpu);
        applications.append(group);
    }

    std::sort(applications.begin(), applications.end(), [](const auto &left, const auto &right) {
        return left.pid < right.pid;
    });
    return applications;
}

bool Monitor::canManage(const ProcessRecord &process)
{
    if (process.members.empty())
        return process.uid == getuid() && process.pid > 1 && process.pid != getpid();
    return std::all_of(process.members.cbegin(), process.members.cend(), [](const ProcessRecord &member) {
        return canManage(member);
    });
}

QVariantMap Monitor::process(int pid, const QString &startTicks) const
{
    const auto record = m_model.find(pid);
    if (!record || QString::number(record->startTicks) != startTicks)
        return {};
    QVariantMap result = ProcessModel::describe(*record);
    result.insert(QStringLiteral("canManage"), canManage(*record));
    return result;
}

QVariantMap Monitor::application(int pid, const QString &startTicks) const
{
    const auto record = m_applicationModel.find(pid);
    if (!record || QString::number(record->startTicks) != startTicks)
        return {};
    QVariantMap result = ProcessModel::describe(*record);
    result.insert(QStringLiteral("canManage"), canManage(*record));
    return result;
}

QString Monitor::endApplication(int pid, const QString &startTicks, bool force)
{
    const auto record = m_applicationModel.find(pid);
    if (!record || QString::number(record->startTicks) != startTicks)
        return tr("The application has already quit.");
    if (!canManage(*record))
        return tr("This application cannot be quit here.");

    // Asking only the processes the application started lets it close its
    // own children; forcing it stops every one of them.
    QSet<int> pids;
    for (const ProcessRecord &member : record->members)
        pids.insert(member.pid);
    QString error;
    bool signalled = false;
    for (const ProcessRecord &member : record->members) {
        if (!force && pids.contains(member.parentPid))
            continue;
        const QString result = signalProcess(member.pid, member.startTicks, force);
        if (result.isEmpty())
            signalled = true;
        else if (result != tr("The process has already exited.") && error.isEmpty())
            error = result;
    }
    if (!error.isEmpty())
        return error;
    if (!signalled)
        return tr("The application has already quit.");
    sample();
    return {};
}

void Monitor::setScope(int scope)
{
    scope = qBound(0, scope, 2);
    if (m_scope == scope)
        return;
    m_scope = scope;
    m_filter.setAllUsers(m_scope == 2);
    savePreferences();
    emit scopeChanged();
}

void Monitor::sortBy(const QString &column)
{
    m_filter.sortBy(column);
    m_applicationFilter.sortBy(column);
    savePreferences();
}

void Monitor::savePreferences() const
{
    QSettings settings(Preferences::ownFile(), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Processes"));
    settings.setValue(QStringLiteral("Scope"), m_scope);
    settings.setValue(QStringLiteral("SortColumn"), m_filter.sortColumn());
    settings.setValue(QStringLiteral("SortAscending"), m_filter.ascending());
}

QString Monitor::endProcess(int pid, const QString &startTicks, bool force)
{
    bool valid = false;
    const quint64 identity = startTicks.toULongLong(&valid);
    if (!valid)
        return tr("The process has already exited.");
    const QString result = signalProcess(pid, identity, force);
    if (result.isEmpty())
        sample();
    return result;
}

QString Monitor::signalProcess(int pid, quint64 startTicks, bool force)
{
    if (pid <= 1 || pid == getpid())
        return tr("This process cannot be ended here.");

    // A pidfd binds the signal to this process even if its PID is reused.
    const int descriptor = syscall(SYS_pidfd_open, pid, 0);
    if (descriptor < 0)
        return errno == ESRCH ? tr("The process has already exited.")
                             : tr("Unable to open the process: %1").arg(QString::fromLocal8Bit(std::strerror(errno)));

    const QString path = QStringLiteral("/proc/%1").arg(pid);
    QFile stat(path + QStringLiteral("/stat"));
    ProcessRecord record;
    QString error;
    if (!stat.open(QIODevice::ReadOnly) || !ProcReader::parseProcess(stat.readAll(), &record, 1)
            || record.startTicks != startTicks) {
        error = tr("The process has already exited.");
    } else if (syscall(SYS_pidfd_send_signal, descriptor, force ? SIGKILL : SIGTERM, nullptr, 0) != 0) {
        const int failure = errno;
        if (failure == ESRCH)
            error = tr("The process has already exited.");
        else if (failure == EPERM)
            error = tr("You can only end your own processes.");
        else
            error = tr("Unable to end the process: %1").arg(QString::fromLocal8Bit(std::strerror(failure)));
    }
    close(descriptor);
    return error;
}

QString Monitor::formatBytes(double bytes) const
{
    const QStringList units { QStringLiteral("B"), QStringLiteral("KiB"), QStringLiteral("MiB"),
                              QStringLiteral("GiB"), QStringLiteral("TiB") };
    int unit = 0;
    bytes = qMax(0.0, bytes);
    while (bytes >= 1024 && unit < units.size() - 1) {
        bytes /= 1024;
        ++unit;
    }
    return QLocale().toString(bytes, 'f', unit == 0 ? 0 : 1) + QLatin1Char(' ') + units[unit];
}

QString Monitor::formatRate(double bytes) const
{
    return formatBytes(bytes) + QStringLiteral("/s");
}

QString Monitor::formatPercent(double percent) const
{
    return QLocale().toString(percent, 'f', 1) + QLatin1Char('%');
}

QString Monitor::formatTemperature(double celsius) const
{
    return QLocale().toString(celsius, 'f', 0) + QStringLiteral(" °C");
}

void Monitor::activateWindow()
{
    emit activationRequested();
}

void Monitor::copyText(const QString &text)
{
    emit copyRequested(text);
}
