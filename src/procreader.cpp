// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "procreader.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStorageInfo>
#include <QSysInfo>

#include <algorithm>
#include <pwd.h>
#include <unistd.h>

static QList<QByteArray> fields(const QByteArray &line)
{
    return line.simplified().split(' ');
}

static quint64 counterDelta(quint64 current, quint64 previous)
{
    return current >= previous ? current - previous : 0;
}

static double byteRate(quint64 current, quint64 previous, double elapsed)
{
    return elapsed > 0 ? counterDelta(current, previous) / elapsed : 0;
}

static bool isScriptInterpreter(const QString &name)
{
    static const QRegularExpression pattern(QStringLiteral(
            "^(python[0-9.]*|perl[0-9.]*|ruby[0-9.]*|node|nodejs|gjs|lua[0-9.]*|sh|bash|dash)$"));
    return pattern.match(name).hasMatch();
}

ProcReader::ProcReader(const QString &procRoot, const QString &sysRoot)
    : m_procRoot(procRoot)
    , m_sysRoot(sysRoot)
{
    m_uid = getuid();
    const long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize > 0)
        m_pageSize = pageSize;
    m_clock.start();

    for (const QByteArray &line : read(m_procRoot + QStringLiteral("/cpuinfo")).split('\n')) {
        if (line.startsWith("model name") || line.startsWith("Hardware")) {
            m_cpuName = QString::fromUtf8(line.mid(line.indexOf(':') + 1).trimmed());
            break;
        }
    }
    if (m_cpuName.isEmpty())
        m_cpuName = QSysInfo::currentCpuArchitecture();
}

QByteArray ProcReader::read(const QString &path) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

CpuCounters ProcReader::parseCpu(const QByteArray &line)
{
    const auto values = fields(line);
    CpuCounters result;
    if (values.size() < 5 || !values[0].startsWith("cpu"))
        return result;

    // Guest counters are already included in user and nice time.
    for (int i = 1; i < qMin(values.size(), 9); ++i)
        result.total += values[i].toULongLong();
    result.idle = values[4].toULongLong();
    if (values.size() > 5)
        result.idle += values[5].toULongLong();
    return result;
}

double ProcReader::cpuUsage(const CpuCounters &current, const CpuCounters &previous)
{
    const quint64 total = counterDelta(current.total, previous.total);
    const quint64 idle = counterDelta(current.idle, previous.idle);
    return total > 0 ? 100.0 * (total - qMin(idle, total)) / total : 0;
}

QVariantMap ProcReader::parseMemory(const QByteArray &contents)
{
    QHash<QByteArray, quint64> values;
    for (const QByteArray &line : contents.split('\n')) {
        const auto parts = fields(line);
        if (parts.size() >= 2)
            values.insert(parts[0].chopped(1), parts[1].toULongLong() * 1024);
    }

    const quint64 total = values.value("MemTotal");
    const quint64 cache = values.value("Cached") + values.value("SReclaimable");
    const quint64 fallback = values.value("MemFree") + values.value("Buffers")
            + cache - qMin(cache, values.value("Shmem"));
    const quint64 available = qMin(total, values.value("MemAvailable", fallback));
    const quint64 swapTotal = values.value("SwapTotal");
    const quint64 swapUsed = swapTotal - qMin(swapTotal, values.value("SwapFree"));

    return {
        { QStringLiteral("memoryTotal"), total },
        { QStringLiteral("memoryUsed"), total - available },
        { QStringLiteral("memoryAvailable"), available },
        { QStringLiteral("memoryCache"), cache },
        { QStringLiteral("memoryPercent"), total > 0 ? 100.0 * (total - available) / total : 0 },
        { QStringLiteral("swapTotal"), swapTotal },
        { QStringLiteral("swapUsed"), swapUsed },
        { QStringLiteral("swapPercent"), swapTotal > 0 ? 100.0 * swapUsed / swapTotal : 0 }
    };
}

bool ProcReader::parseProcess(const QByteArray &contents, ProcessRecord *process, qint64 pageSize)
{
    const int opening = contents.indexOf('(');
    const int closing = contents.lastIndexOf(')');
    if (opening < 1 || closing <= opening)
        return false;

    const auto values = fields(contents.mid(closing + 1));
    if (values.size() < 22)
        return false;

    bool validPid = false;
    bool validStart = false;
    process->pid = contents.left(opening).trimmed().toInt(&validPid);
    process->name = QString::fromUtf8(contents.mid(opening + 1, closing - opening - 1));
    process->state = QString::fromLatin1(values[0]);
    process->parentPid = values[1].toInt();
    process->kernelThread = values[6].toUInt() & 0x00200000; // PF_KTHREAD
    process->cpuTicks = values[11].toULongLong() + values[12].toULongLong();
    process->nice = values[16].toInt();
    process->threads = values[17].toInt();
    process->startTicks = values[19].toULongLong(&validStart);
    process->memory = qMax<qint64>(0, values[21].toLongLong()) * pageSize;
    return validPid && validStart && process->pid > 0;
}

QString ProcReader::stateName(const QString &state)
{
    if (state == QLatin1String("R"))
        return QCoreApplication::translate("Monitor", "Running");
    if (state == QLatin1String("S") || state == QLatin1String("I"))
        return QCoreApplication::translate("Monitor", "Sleeping");
    if (state == QLatin1String("D"))
        return QCoreApplication::translate("Monitor", "Waiting for I/O");
    if (state == QLatin1String("T") || state == QLatin1String("t"))
        return QCoreApplication::translate("Monitor", "Stopped");
    if (state == QLatin1String("Z"))
        return QCoreApplication::translate("Monitor", "Zombie");
    return state;
}

bool ProcReader::parseUid(const QByteArray &status, uint *uid)
{
    for (const QByteArray &line : status.split('\n')) {
        if (!line.startsWith("Uid:"))
            continue;
        // Real, effective, saved and filesystem IDs; ownership follows the effective one.
        const auto values = fields(line);
        bool valid = false;
        *uid = values.value(2).toUInt(&valid);
        return valid;
    }
    return false;
}

QString ProcReader::applicationProgram(const QByteArray &commandLine)
{
    const QList<QByteArray> arguments = commandLine.split('\0');
    QString program = QString::fromLocal8Bit(arguments.value(0));

    // A script started through its shebang runs as the interpreter, which
    // would match every application written in that language. The script
    // names the application; inline code (-c, -e) or a module names none.
    if (isScriptInterpreter(QFileInfo(program).fileName())) {
        program.clear();
        for (qsizetype i = 1; i < arguments.size(); ++i) {
            const QByteArray &argument = arguments.at(i);
            if (argument.isEmpty() || argument == "-c" || argument == "-e" || argument == "-m")
                break;
            if (argument.startsWith('-'))
                continue;

            program = QString::fromLocal8Bit(argument);
            break;
        }
    }

    return program;
}

QString ProcReader::scopeApplication(const QByteArray &cgroup)
{
    QByteArray path;
    for (const QByteArray &line : cgroup.split('\n')) {
        if (line.startsWith("0::"))
            path = line.mid(3);
    }

    // Launchers name units app[-<launcher>]-<id>-<random>.scope or
    // app[-<launcher>]-<id>[@<random>].service, with '-' in the id escaped.
    const QByteArray unit = path.mid(path.lastIndexOf('/') + 1);
    if (!unit.startsWith("app-"))
        return {};
    QByteArray id;
    if (unit.endsWith(".scope")) {
        const QList<QByteArray> parts = unit.chopped(6).split('-');
        if (parts.size() < 3)
            return {};
        id = parts.at(parts.size() - 2);
    } else if (unit.endsWith(".service")) {
        const QList<QByteArray> parts = unit.chopped(8).split('-');
        if (parts.size() < 2)
            return {};
        id = parts.last();
        const int instance = id.indexOf('@');
        if (instance >= 0)
            id.truncate(instance);
    } else {
        return {};
    }

    QByteArray unescaped;
    for (int i = 0; i < id.size(); ++i) {
        bool valid = false;
        const int value = id.mid(i + 2, 2).toInt(&valid, 16);
        if (id.at(i) == '\\' && i + 3 < id.size() && id.at(i + 1) == 'x' && valid) {
            unescaped.append(char(value));
            i += 3;
        } else {
            unescaped.append(id.at(i));
        }
    }
    return QString::fromUtf8(unescaped);
}

bool ProcReader::parsePss(const QByteArray &rollup, quint64 *pss)
{
    for (const QByteArray &line : rollup.split('\n')) {
        if (!line.startsWith("Pss:"))
            continue;
        bool valid = false;
        *pss = fields(line).value(1).toULongLong(&valid) * 1024;
        return valid;
    }
    return false;
}

void ProcReader::readIdentity(const QString &path, ProcessRecord *process) const
{
    process->executable = QFileInfo(path + QStringLiteral("/exe")).symLinkTarget();
    QString executable = process->executable;
    if (executable.endsWith(QLatin1String(" (deleted)")) && !QFileInfo::exists(executable))
        executable.chop(10);

    const QByteArray commandLine = read(path + QStringLiteral("/cmdline"));
    process->program = commandLine.isEmpty() ? executable : applicationProgram(commandLine);
    QByteArray command = commandLine;
    command.replace('\0', ' ');
    process->command = QString::fromLocal8Bit(command).trimmed();

    const QString executableName = QFileInfo(executable).fileName();
    process->displayName = executableName.isEmpty() ? process->name : executableName;
    process->iconName = QStringLiteral("application-x-executable");
}

QVariantList ProcReader::volumes()
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

void ProcReader::scanGpus()
{
    m_gpusScanned = true;
    static const QRegularExpression card(QStringLiteral("^card[0-9]+$"));
    const QDir drm(m_sysRoot + QStringLiteral("/class/drm"));
    for (const QString &entry : drm.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        if (!card.match(entry).hasMatch())
            continue;

        Gpu gpu;
        gpu.device = drm.filePath(entry) + QStringLiteral("/device");
        const QDir hwmon(gpu.device + QStringLiteral("/hwmon"));
        const QStringList monitors = hwmon.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        if (!monitors.isEmpty())
            gpu.hwmon = hwmon.filePath(monitors.first());
        gpu.hasBusy = QFileInfo::exists(gpu.device + QStringLiteral("/gpu_busy_percent"));
        gpu.hasTemperature = !gpu.hwmon.isEmpty() && QFileInfo::exists(gpu.hwmon + QStringLiteral("/temp1_input"));
        gpu.hasFrequency = !gpu.hwmon.isEmpty() && QFileInfo::exists(gpu.hwmon + QStringLiteral("/freq1_input"));
        gpu.hasVram = QFileInfo::exists(gpu.device + QStringLiteral("/mem_info_vram_total"));
        if (!gpu.hasBusy && !gpu.hasTemperature)
            continue;

        gpu.primary = read(gpu.device + QStringLiteral("/boot_vga")).trimmed() == "1";
        gpu.driver = QFileInfo(QFileInfo(gpu.device + QStringLiteral("/driver")).symLinkTarget()).fileName();
        gpu.name = QString::fromUtf8(read(gpu.device + QStringLiteral("/product_name")).trimmed());
        if (gpu.name.isEmpty())
            gpu.name = pciName(read(gpu.device + QStringLiteral("/vendor")).trimmed(),
                               read(gpu.device + QStringLiteral("/device")).trimmed());
        if (gpu.name.isEmpty())
            gpu.name = gpu.driver.isEmpty() ? entry : gpu.driver;
        m_gpus.append(gpu);
    }
    std::stable_sort(m_gpus.begin(), m_gpus.end(), [](const Gpu &left, const Gpu &right) {
        return left.primary && !right.primary;
    });
}

QString ProcReader::pciName(const QByteArray &vendor, const QByteArray &device) const
{
    const QByteArray vendorId = vendor.startsWith("0x") ? vendor.mid(2).toLower() : vendor.toLower();
    const QByteArray deviceId = device.startsWith("0x") ? device.mid(2).toLower() : device.toLower();
    if (vendorId.size() != 4 || deviceId.size() != 4)
        return {};

    for (const QString &path : { QStringLiteral("/usr/share/misc/pci.ids"), QStringLiteral("/usr/share/hwdata/pci.ids") }) {
        const QByteArray ids = read(path);
        if (ids.isEmpty())
            continue;
        bool inVendor = false;
        for (const QByteArray &line : ids.split('\n')) {
            if (line.isEmpty() || line.startsWith('#'))
                continue;
            if (line.at(0) != '\t') {
                if (inVendor)
                    return {};
                inVendor = line.startsWith(vendorId + "  ");
            } else if (inVendor && line.startsWith("\t" + deviceId + "  ")) {
                return QString::fromUtf8(line.mid(7).trimmed());
            }
        }
        return {};
    }
    return {};
}

void ProcReader::sampleGpus(QVariantMap *metrics)
{
    if (!m_gpusScanned)
        scanGpus();

    QVariantList gpus;
    for (const Gpu &gpu : std::as_const(m_gpus)) {
        // Reading a runtime-suspended GPU's counters would wake it up.
        const bool active = read(gpu.device + QStringLiteral("/power/runtime_status")).trimmed() != "suspended";
        bool valid = false;
        int busy = 0;
        double temperature = -1;
        double frequency = 0;
        quint64 vramUsed = 0;
        quint64 vramTotal = 0;
        if (active && gpu.hasBusy)
            busy = qBound(0, read(gpu.device + QStringLiteral("/gpu_busy_percent")).trimmed().toInt(), 100);
        if (active && gpu.hasTemperature) {
            const qint64 value = read(gpu.hwmon + QStringLiteral("/temp1_input")).trimmed().toLongLong(&valid);
            if (valid)
                temperature = value / 1000.0;
        }
        if (active && gpu.hasFrequency)
            frequency = read(gpu.hwmon + QStringLiteral("/freq1_input")).trimmed().toULongLong() / 1e6;
        if (active && gpu.hasVram) {
            vramUsed = read(gpu.device + QStringLiteral("/mem_info_vram_used")).trimmed().toULongLong();
            vramTotal = read(gpu.device + QStringLiteral("/mem_info_vram_total")).trimmed().toULongLong();
        }
        gpus.append(QVariantMap {
            { QStringLiteral("name"), gpu.name },
            { QStringLiteral("driver"), gpu.driver },
            { QStringLiteral("active"), active },
            { QStringLiteral("hasBusy"), gpu.hasBusy },
            { QStringLiteral("busy"), busy },
            { QStringLiteral("hasTemperature"), gpu.hasTemperature },
            { QStringLiteral("temperature"), temperature },
            { QStringLiteral("hasFrequency"), gpu.hasFrequency },
            { QStringLiteral("frequency"), frequency },
            { QStringLiteral("hasVram"), gpu.hasVram },
            { QStringLiteral("vramUsed"), vramUsed },
            { QStringLiteral("vramTotal"), vramTotal }
        });
    }

    // The overview follows the primary GPU, or the first one with an activity counter.
    QVariantMap main = gpus.value(0).toMap();
    for (const QVariant &gpu : std::as_const(gpus)) {
        if (gpu.toMap().value(QStringLiteral("hasBusy")).toBool()) {
            if (!main.value(QStringLiteral("hasBusy")).toBool())
                main = gpu.toMap();
            break;
        }
    }
    metrics->insert(QStringLiteral("gpus"), gpus);
    metrics->insert(QStringLiteral("gpuAvailable"), !gpus.isEmpty());
    metrics->insert(QStringLiteral("gpuName"), main.value(QStringLiteral("name")).toString());
    metrics->insert(QStringLiteral("gpuHasBusy"), main.value(QStringLiteral("hasBusy")).toBool());
    metrics->insert(QStringLiteral("gpuHasTemperature"), main.value(QStringLiteral("hasTemperature")).toBool());
    metrics->insert(QStringLiteral("gpuPercent"), main.value(QStringLiteral("busy")).toDouble());
    metrics->insert(QStringLiteral("gpuTemperature"), qMax(0.0, main.value(QStringLiteral("temperature")).toDouble()));
}

QString ProcReader::userName(uint uid)
{
    if (!m_users.contains(uid)) {
        QByteArray buffer(16384, '\0');
        passwd entry;
        passwd *result = nullptr;
        const int error = getpwuid_r(uid, &entry, buffer.data(), buffer.size(), &result);
        m_users.insert(uid, error == 0 && result ? QString::fromLocal8Bit(entry.pw_name)
                                               : QString::number(uid));
    }
    return m_users.value(uid);
}

Snapshot ProcReader::sample(const QSet<int> &applicationPids)
{
    Snapshot snapshot;
    const QByteArray stat = read(m_procRoot + QStringLiteral("/stat"));
    const QByteArray memory = read(m_procRoot + QStringLiteral("/meminfo"));
    if (!stat.startsWith("cpu ") || memory.isEmpty()) {
        snapshot.error = QCoreApplication::translate("Monitor", "System information is unavailable.");
        return snapshot;
    }

    const qint64 now = m_clock.nsecsElapsed();
    const double elapsed = m_hasPreviousSample ? (now - m_previousTime) / 1e9 : 0;
    snapshot.metrics = parseMemory(memory);
    QHash<QString, CpuCounters> cpus;
    QVariantList coreUsage;
    for (const QByteArray &line : stat.split('\n')) {
        if (!line.startsWith("cpu"))
            continue;
        const QString name = QString::fromLatin1(fields(line).first());
        const CpuCounters counters = parseCpu(line);
        cpus.insert(name, counters);
        const double usage = m_previousCpu.contains(name) ? cpuUsage(counters, m_previousCpu.value(name)) : 0;
        if (name == QLatin1String("cpu"))
            snapshot.metrics.insert(QStringLiteral("cpuPercent"), usage);
        else
            coreUsage.append(usage);
    }
    snapshot.metrics.insert(QStringLiteral("cores"), coreUsage);
    snapshot.metrics.insert(QStringLiteral("coreCount"), coreUsage.size());
    snapshot.metrics.insert(QStringLiteral("cpuName"), m_cpuName);

    const quint64 totalDelta = counterDelta(cpus.value(QStringLiteral("cpu")).total,
                                           m_previousCpu.value(QStringLiteral("cpu")).total);
    QHash<int, ProcessRecord> processes;
    const QDir procDirectory(m_procRoot);
    for (const QString &directory : procDirectory.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool numeric = false;
        directory.toInt(&numeric);
        if (!numeric)
            continue;

        const QString path = procDirectory.filePath(directory);
        ProcessRecord process;
        if (!parseProcess(read(path + QStringLiteral("/stat")), &process, m_pageSize))
            continue;

        // A non-dumpable process's directory belongs to root, so status has to
        // name the owner of a root-owned one. It is costly to generate.
        process.uid = QFileInfo(path).ownerId();
        if (process.uid == 0 && !process.kernelThread
                && !parseUid(read(path + QStringLiteral("/status")), &process.uid))
            continue;
        process.user = userName(process.uid);

        const QByteArray io = read(path + QStringLiteral("/io"));
        bool hasRead = false;
        bool hasWrite = false;
        for (const QByteArray &line : io.split('\n')) {
            const auto values = fields(line);
            if (values.size() < 2)
                continue;
            if (values[0] == "read_bytes:") {
                process.readBytes = values[1].toULongLong();
                hasRead = true;
            } else if (values[0] == "write_bytes:") {
                process.writeBytes = values[1].toULongLong();
                hasWrite = true;
            }
        }
        process.ioAvailable = hasRead && hasWrite;

        // Identity is read once per process. A second stat read proves the
        // files above did not come from a process that reused the PID.
        const ProcessRecord previous = m_previousProcesses.value(process.pid);
        if (previous.pid == process.pid && previous.startTicks == process.startTicks) {
            process.executable = previous.executable;
            process.program = previous.program;
            process.command = previous.command;
            process.displayName = previous.displayName;
            process.iconName = previous.iconName;
            process.scopeId = previous.scopeId;
            process.pss = previous.pss;
            process.pssTime = previous.pssTime;
            process.pssAvailable = previous.pssAvailable;
            // The launcher moves a new process into its scope after starting
            // it, so the first read can predate that; the second one settles.
            process.scopeSettled = true;
            if (!previous.scopeSettled && process.uid == m_uid && !process.kernelThread)
                process.scopeId = scopeApplication(read(path + QStringLiteral("/cgroup")));
            if (totalDelta > 0)
                process.cpu = qMin(100.0, 100.0 * counterDelta(process.cpuTicks, previous.cpuTicks) / totalDelta);
            if (previous.ioAvailable && process.ioAvailable) {
                process.readRate = byteRate(process.readBytes, previous.readBytes, elapsed);
                process.writeRate = byteRate(process.writeBytes, previous.writeBytes, elapsed);
            }
        } else {
            readIdentity(path, &process);
            if (process.uid == m_uid && !process.kernelThread)
                process.scopeId = scopeApplication(read(path + QStringLiteral("/cgroup")));
            ProcessRecord verified;
            if (!parseProcess(read(path + QStringLiteral("/stat")), &verified, m_pageSize)
                    || verified.startTicks != process.startTicks)
                continue;
        }
        processes.insert(process.pid, process);
        snapshot.processes.append(process);
    }
    std::sort(snapshot.processes.begin(), snapshot.processes.end(), [](const auto &left, const auto &right) {
        return left.pid < right.pid;
    });
    snapshot.metrics.insert(QStringLiteral("processCount"), snapshot.processes.size());

    // Shared pages count once across an application's processes through PSS.
    // The kernel walks every page table to report it, so it is refreshed
    // every few seconds rather than on every sample.
    QSet<int> measured = applicationPids;
    for (ProcessRecord &process : snapshot.processes) {
        const bool member = process.uid == m_uid && !process.kernelThread
                && (measured.contains(process.pid) || measured.contains(process.parentPid) || !process.scopeId.isEmpty());
        if (!member) {
            process.pssAvailable = false;
            continue;
        }
        measured.insert(process.pid);
        if (process.pssAvailable && now - process.pssTime < 5'000'000'000)
            continue;
        process.pssAvailable = parsePss(read(m_procRoot + QLatin1Char('/') + QString::number(process.pid)
                                             + QStringLiteral("/smaps_rollup")), &process.pss);
        process.pssTime = now;
        ProcessRecord &retained = processes[process.pid];
        retained.pss = process.pss;
        retained.pssTime = process.pssTime;
        retained.pssAvailable = process.pssAvailable;
    }

    QHash<QString, QPair<quint64, quint64>> networks;
    QVariantList interfaces;
    double download = 0;
    double upload = 0;
    quint64 received = 0;
    quint64 sent = 0;
    for (const QByteArray &line : read(m_procRoot + QStringLiteral("/net/dev")).split('\n')) {
        const int separator = line.indexOf(':');
        if (separator < 0)
            continue;
        const QString name = QString::fromLatin1(line.left(separator).trimmed());
        const auto values = fields(line.mid(separator + 1));
        if (name == QLatin1String("lo") || values.size() < 16)
            continue;
        const quint64 rx = values[0].toULongLong();
        const quint64 tx = values[8].toULongLong();
        networks.insert(name, { rx, tx });
        const auto previous = m_previousNetworks.value(name);
        const double rxRate = m_previousNetworks.contains(name) ? byteRate(rx, previous.first, elapsed) : 0;
        const double txRate = m_previousNetworks.contains(name) ? byteRate(tx, previous.second, elapsed) : 0;
        const bool physical = QFileInfo::exists(m_sysRoot + QStringLiteral("/class/net/") + name + QStringLiteral("/device"));
        if (physical) {
            download += rxRate;
            upload += txRate;
            received += rx;
            sent += tx;
        }
        interfaces.append(QVariantMap {
            { QStringLiteral("name"), name },
            { QStringLiteral("state"), QString::fromLatin1(read(m_sysRoot + QStringLiteral("/class/net/") + name + QStringLiteral("/operstate")).trimmed()) },
            { QStringLiteral("address"), QString::fromLatin1(read(m_sysRoot + QStringLiteral("/class/net/") + name + QStringLiteral("/address")).trimmed()) },
            { QStringLiteral("download"), rxRate },
            { QStringLiteral("upload"), txRate },
            { QStringLiteral("received"), rx },
            { QStringLiteral("sent"), tx },
            { QStringLiteral("physical"), physical }
        });
    }
    snapshot.metrics.insert(QStringLiteral("download"), download);
    snapshot.metrics.insert(QStringLiteral("upload"), upload);
    snapshot.metrics.insert(QStringLiteral("received"), received);
    snapshot.metrics.insert(QStringLiteral("sent"), sent);
    snapshot.metrics.insert(QStringLiteral("interfaces"), interfaces);

    QHash<QString, QPair<quint64, quint64>> disks;
    QVariantList diskDevices;
    double diskRead = 0;
    double diskWrite = 0;
    for (const QByteArray &line : read(m_procRoot + QStringLiteral("/diskstats")).split('\n')) {
        const auto values = fields(line);
        if (values.size() < 14)
            continue;
        const QString name = QString::fromLatin1(values[2]);
        const QString path = m_sysRoot + QStringLiteral("/block/") + name;
        if (!QFileInfo::exists(path + QStringLiteral("/device")))
            continue;
        const quint64 readBytes = values[5].toULongLong() * 512;
        const quint64 writeBytes = values[9].toULongLong() * 512;
        disks.insert(name, { readBytes, writeBytes });
        const auto previous = m_previousDisks.value(name);
        const double readRate = m_previousDisks.contains(name) ? byteRate(readBytes, previous.first, elapsed) : 0;
        const double writeRate = m_previousDisks.contains(name) ? byteRate(writeBytes, previous.second, elapsed) : 0;
        diskRead += readRate;
        diskWrite += writeRate;
        diskDevices.append(QVariantMap {
            { QStringLiteral("name"), name },
            { QStringLiteral("model"), QString::fromUtf8(read(path + QStringLiteral("/device/model")).trimmed()) },
            { QStringLiteral("read"), readRate },
            { QStringLiteral("write"), writeRate },
            { QStringLiteral("size"), read(path + QStringLiteral("/size")).trimmed().toULongLong() * 512 }
        });
    }
    snapshot.metrics.insert(QStringLiteral("diskRead"), diskRead);
    snapshot.metrics.insert(QStringLiteral("diskWrite"), diskWrite);
    snapshot.metrics.insert(QStringLiteral("disks"), diskDevices);
    sampleGpus(&snapshot.metrics);

    m_previousCpu = cpus;
    m_previousProcesses = processes;
    m_previousNetworks = networks;
    m_previousDisks = disks;
    m_previousTime = now;
    snapshot.baseline = !m_hasPreviousSample;
    m_hasPreviousSample = true;
    snapshot.available = true;
    return snapshot;
}
