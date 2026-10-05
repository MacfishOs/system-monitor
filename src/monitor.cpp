// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "monitor.h"

#include "applicationregistry.h"
#include "desktopentry.h"

#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QtConcurrent>

#include <cerrno>
#include <cstring>
#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>

Monitor::Monitor(QObject *parent)
    : QObject(parent)
    , m_model(this)
    , m_filter(&m_model, this)
    , m_reader(std::make_shared<ProcReader>())
{
    m_metrics = ProcReader::parseMemory({});
    for (const QString &key : { QStringLiteral("cpuPercent"), QStringLiteral("download"),
                              QStringLiteral("upload"), QStringLiteral("diskRead"),
                              QStringLiteral("diskWrite"),
                              QStringLiteral("received"), QStringLiteral("sent"),
                              QStringLiteral("processCount"), QStringLiteral("coreCount") })
        m_metrics.insert(key, 0);
    for (const QString &key : { QStringLiteral("cores"), QStringLiteral("interfaces"),
                              QStringLiteral("disks"), QStringLiteral("volumes") })
        m_metrics.insert(key, QVariantList());
    m_metrics.insert(QStringLiteral("cpuName"), QString());
    m_historyClock.start();
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &Monitor::sample);
    connect(&m_watcher, &QFutureWatcher<Snapshot>::finished, this, &Monitor::acceptSample);
    connect(&m_volumeWatcher, &QFutureWatcher<QVariantList>::finished, this, [this] {
        m_volumes = m_volumeWatcher.result();
    });
    connect(ApplicationRegistry::instance(), &ApplicationRegistry::applicationsChanged, this, [this] {
        m_applications.clear();
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
        m_volumeWatcher.setFuture(QtConcurrent::run(&ProcReader::volumes));

    if (m_watcher.isRunning())
        return;
    const auto reader = m_reader;
    m_watcher.setFuture(QtConcurrent::run([reader] {
        return reader->sample();
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
        const qint64 timestamp = m_historyClock.elapsed();
        m_historyTimes.append(timestamp);
        for (const QString &key : { QStringLiteral("cpuPercent"), QStringLiteral("memoryPercent"),
                                  QStringLiteral("download"), QStringLiteral("upload"),
                                  QStringLiteral("diskRead"), QStringLiteral("diskWrite") }) {
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
                                            entry ? Application { entry->name(), entry->icon() } : Application());
    }
    if (!application->name.isEmpty())
        process->displayName = application->name;
    if (!application->icon.isEmpty())
        process->iconName = application->icon;
}

QVariantMap Monitor::process(int pid, const QString &startTicks) const
{
    const auto record = m_model.find(pid);
    if (!record || QString::number(record->startTicks) != startTicks)
        return {};
    QVariantMap result = ProcessModel::describe(*record);
    result.insert(QStringLiteral("canManage"), record->uid == getuid()
                  && pid > 1 && pid != getpid());
    return result;
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

void Monitor::activateWindow()
{
    emit activationRequested();
}

void Monitor::copyText(const QString &text)
{
    emit copyRequested(text);
}
