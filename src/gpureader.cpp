// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gpureader.h"
#include "fileutil.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#include <algorithm>
#include <dirent.h>
#include <unistd.h>

GpuReader::GpuReader(const QString &procRoot, const QString &sysRoot)
    : m_procRoot(procRoot)
    , m_sysRoot(sysRoot)
    , m_uid(getuid())
{
}

void GpuReader::scan()
{
    m_scanned = true;
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
        gpu.nodes = QDir(gpu.device + QStringLiteral("/drm")).entryList({ QStringLiteral("card*"), QStringLiteral("renderD*") },
                                                                        QDir::Dirs | QDir::NoDotAndDotDot);
        if (!gpu.hasBusy && !gpu.hasTemperature && gpu.nodes.isEmpty())
            continue;

        gpu.primary = readFile(gpu.device + QStringLiteral("/boot_vga")).trimmed() == "1";
        gpu.driver = QFileInfo(QFileInfo(gpu.device + QStringLiteral("/driver")).symLinkTarget()).fileName();
        gpu.name = QString::fromUtf8(readFile(gpu.device + QStringLiteral("/product_name")).trimmed());
        if (gpu.name.isEmpty())
            gpu.name = pciName(readFile(gpu.device + QStringLiteral("/vendor")).trimmed(),
                               readFile(gpu.device + QStringLiteral("/device")).trimmed());
        if (gpu.name.isEmpty())
            gpu.name = gpu.driver.isEmpty() ? entry : gpu.driver;
        m_gpus.append(gpu);
    }
    std::stable_sort(m_gpus.begin(), m_gpus.end(), [](const Gpu &left, const Gpu &right) {
        return left.primary && !right.primary;
    });
    for (int index = 0; index < m_gpus.size(); ++index) {
        if (m_gpus[index].hasBusy)
            continue;
        for (const QString &node : std::as_const(m_gpus[index].nodes))
            m_nodeOwners.insert("/dev/dri/" + QFile::encodeName(node), index);
    }
}

QString GpuReader::pciName(const QByteArray &vendor, const QByteArray &device)
{
    const QByteArray vendorId = vendor.startsWith("0x") ? vendor.mid(2).toLower() : vendor.toLower();
    const QByteArray deviceId = device.startsWith("0x") ? device.mid(2).toLower() : device.toLower();
    if (vendorId.size() != 4 || deviceId.size() != 4)
        return {};

    for (const QString &path : { QStringLiteral("/usr/share/misc/pci.ids"), QStringLiteral("/usr/share/hwdata/pci.ids") }) {
        const QByteArray ids = readFile(path);
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

DrmClient GpuReader::parseDrmFdinfo(const QByteArray &contents)
{
    DrmClient client;
    QHash<QByteArray, int> capacities;
    for (const QByteArray &line : contents.split('\n')) {
        const int colon = line.indexOf(':');
        if (colon < 0)
            continue;
        const QByteArray key = line.left(colon);
        const QByteArray value = splitFields(line.mid(colon + 1)).value(0);
        if (key == "drm-client-id") {
            client.id = value;
            continue;
        }

        bool valid = false;
        const quint64 number = value.toULongLong(&valid);
        if (!valid)
            continue;
        if (key.startsWith("drm-engine-capacity-")) {
            capacities.insert(key.mid(20), qMax(1, int(number)));
        } else if (key.startsWith("drm-engine-")) {
            DrmEngine &engine = client.engines[key.mid(11)];
            engine.busyTime = number;
            engine.hasTime = true;
        } else if (key.startsWith("drm-total-cycles-")) {
            client.engines[key.mid(17)].totalCycles = number;
        } else if (key.startsWith("drm-cycles-")) {
            DrmEngine &engine = client.engines[key.mid(11)];
            engine.cycles = number;
            engine.hasCycles = true;
        }
    }
    for (auto engine = client.engines.begin(); engine != client.engines.end(); ++engine)
        engine->capacity = capacities.value(engine.key(), 1);
    return client;
}

double GpuReader::drmBusy(const QHash<QByteArray, DrmClient> &current,
                           const QHash<QByteArray, DrmClient> &previous, double elapsedTime)
{
    QHash<QByteArray, double> loads;
    QHash<QByteArray, int> capacities;
    for (auto client = current.cbegin(); client != current.cend(); ++client) {
        // A new client's counters include work done before the interval.
        const auto earlier = previous.constFind(client.key());
        if (earlier == previous.cend())
            continue;

        for (auto engine = client->engines.cbegin(); engine != client->engines.cend(); ++engine) {
            const DrmEngine before = earlier->engines.value(engine.key());
            double load = 0;
            if (engine->hasCycles && before.hasCycles && engine->totalCycles > before.totalCycles)
                load = double(counterDelta(engine->cycles, before.cycles)) / (engine->totalCycles - before.totalCycles);
            else if (engine->hasTime && before.hasTime && elapsedTime > 0)
                load = counterDelta(engine->busyTime, before.busyTime) / elapsedTime;
            loads[engine.key()] += load;
            capacities[engine.key()] = qMax(capacities.value(engine.key(), 1), engine->capacity);
        }
    }

    // The busiest engine sets the GPU's activity.
    double busy = 0;
    for (auto load = loads.cbegin(); load != loads.cend(); ++load)
        busy = qMax(busy, load.value() / capacities.value(load.key()));
    return qBound(0.0, 100.0 * busy, 100.0);
}

QVector<QPair<QByteArray, int>> GpuReader::drmDescriptors(const QString &path) const
{
    QVector<QPair<QByteArray, int>> nodes;
    DIR *descriptors = opendir(QFile::encodeName(path + QStringLiteral("/fd")).constData());
    if (!descriptors)
        return nodes;
    while (const dirent *entry = readdir(descriptors)) {
        char target[256];
        const ssize_t length = readlinkat(dirfd(descriptors), entry->d_name, target, sizeof(target));
        if (length <= 0 || length == sizeof(target))
            continue;
        const auto owner = m_nodeOwners.constFind(QByteArray(target, length));
        if (owner != m_nodeOwners.cend())
            nodes.append({ QByteArray(entry->d_name), *owner });
    }
    closedir(descriptors);
    return nodes;
}

QVector<QHash<QByteArray, DrmClient>> GpuReader::drmClients(const QVector<ProcessRecord> &processes,
                                                             const QVector<bool> &wanted)
{
    QVector<QHash<QByteArray, DrmClient>> clients(m_gpus.size());
    if (!wanted.contains(true))
        return clients;

    ++m_samples;
    QHash<int, Descriptors> listed;
    for (const ProcessRecord &process : processes) {
        // Another user's descriptors cannot be inspected.
        if (process.uid != m_uid || process.kernelThread)
            continue;

        // Listing descriptors costs about 10 µs each, so a process's are listed
        // when it appears and every fifth sample after, spread across processes.
        const QString path = m_procRoot + QLatin1Char('/') + QString::number(process.pid);
        Descriptors descriptors = m_descriptors.value(process.pid);
        if (descriptors.startTicks != process.startTicks || (m_samples + process.pid) % 5 == 0)
            descriptors = { process.startTicks, drmDescriptors(path) };
        listed.insert(process.pid, descriptors);

        for (const auto &[descriptor, gpu] : std::as_const(descriptors.nodes)) {
            if (!wanted[gpu])
                continue;
            // A closed descriptor's number may now name another file, which has no DRM fdinfo.
            const DrmClient client = parseDrmFdinfo(readFile(path + QStringLiteral("/fdinfo/") + QString::fromLatin1(descriptor)));
            if (client.engines.isEmpty())
                continue;
            // Descriptors shared through fork or dup belong to one client.
            const QByteArray key = client.id.isEmpty() ? QByteArray::number(process.pid) + ':' + descriptor : client.id;
            clients[gpu].insert(key, client);
        }
    }
    m_descriptors = listed;
    return clients;
}

void GpuReader::sample(QVariantMap *metrics, const QVector<ProcessRecord> &processes, double elapsed)
{
    if (!m_scanned)
        scan();

    // Reading a runtime-suspended GPU's counters would wake it up.
    QVector<bool> active(m_gpus.size());
    QVector<bool> clientCounted(m_gpus.size());
    for (int index = 0; index < m_gpus.size(); ++index) {
        const Gpu &gpu = m_gpus[index];
        active[index] = readFile(gpu.device + QStringLiteral("/power/runtime_status")).trimmed() != "suspended";
        clientCounted[index] = active[index] && !gpu.hasBusy && !gpu.nodes.isEmpty();
    }
    const auto clients = drmClients(processes, clientCounted);

    QVariantList gpus;
    for (int index = 0; index < m_gpus.size(); ++index) {
        Gpu &gpu = m_gpus[index];
        int busy = 0;
        // Without a device-wide counter, activity sums the busy time that
        // DRM fdinfo reports for each of the user's clients.
        if (clientCounted[index]) {
            if (!clients[index].isEmpty())
                gpu.hasClientBusy = true;
            busy = qRound(drmBusy(clients[index], gpu.clients, elapsed * 1e9));
            gpu.clients = clients[index];
        }
        if (!gpu.hasBusy && !gpu.hasClientBusy && !gpu.hasTemperature)
            continue;

        const bool hasBusy = gpu.hasBusy || gpu.hasClientBusy;
        bool valid = false;
        double temperature = -1;
        double frequency = 0;
        quint64 vramUsed = 0;
        quint64 vramTotal = 0;
        if (active[index] && gpu.hasBusy)
            busy = qBound(0, readFile(gpu.device + QStringLiteral("/gpu_busy_percent")).trimmed().toInt(), 100);
        if (active[index] && gpu.hasTemperature) {
            const qint64 value = readFile(gpu.hwmon + QStringLiteral("/temp1_input")).trimmed().toLongLong(&valid);
            if (valid)
                temperature = value / 1000.0;
        }
        if (active[index] && gpu.hasFrequency)
            frequency = readFile(gpu.hwmon + QStringLiteral("/freq1_input")).trimmed().toULongLong() / 1e6;
        if (active[index] && gpu.hasVram) {
            vramUsed = readFile(gpu.device + QStringLiteral("/mem_info_vram_used")).trimmed().toULongLong();
            vramTotal = readFile(gpu.device + QStringLiteral("/mem_info_vram_total")).trimmed().toULongLong();
        }
        gpus.append(QVariantMap {
            { QStringLiteral("name"), gpu.name },
            { QStringLiteral("driver"), gpu.driver },
            { QStringLiteral("active"), active[index] },
            { QStringLiteral("hasBusy"), hasBusy },
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
