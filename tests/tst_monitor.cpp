// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "applicationregistry.h"
#include "desktopentry.h"
#include "monitor.h"
#include "procreader.h"

#include <QAbstractItemModelTester>
#include <QDir>
#include <QFile>
#include <QPersistentModelIndex>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <unistd.h>

static QByteArray processStat(int pid, const QString &name, quint64 ticks, quint64 start, uint flags = 0)
{
    QStringList fields;
    for (int i = 0; i < 22; ++i)
        fields.append(QStringLiteral("0"));
    fields[0] = QStringLiteral("S");
    fields[1] = QStringLiteral("42");
    fields[6] = QString::number(flags);
    fields[11] = QString::number(ticks);
    fields[16] = QStringLiteral("5");
    fields[17] = QStringLiteral("3");
    fields[19] = QString::number(start);
    fields[21] = QStringLiteral("7");
    return (QStringLiteral("%1 (%2) ").arg(pid).arg(name) + fields.join(QLatin1Char(' '))).toUtf8();
}

static QByteArray commandLine(const QList<QByteArray> &arguments)
{
    QByteArray result;
    for (const QByteArray &argument : arguments)
        result += argument + '\0';
    return result;
}

static bool writeFile(const QString &path, const QByteArray &contents)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}

static ProcessRecord record(int pid, const QString &name, double cpu = 0)
{
    ProcessRecord process;
    process.pid = pid;
    process.uid = getuid();
    process.name = name;
    process.displayName = name;
    process.command = QStringLiteral("/usr/bin/") + name + QStringLiteral(" --example");
    process.user = QStringLiteral("test-user");
    process.startTicks = 1000;
    process.cpu = cpu;
    process.memory = pid * 1024;
    return process;
}

class MonitorTest : public QObject
{
    Q_OBJECT

private slots:
    void cpuAccounting();
    void memoryAccounting();
    void processNamesAndFields();
    void samplingAndPidReuse();
    void deviceAccounting();
    void unavailableProc();
    void modelUpdatesPreserveIdentity();
    void filteringAndSorting();
    void terminateChild_data();
    void terminateChild();
    void rejectReusedAndProtectedPid();
    void rejectForeignProcess();
    void desktopIdentity();
    void scopesAndPss();
    void sampledScopesAndPss();
    void applicationGroups();
    void gpuAccounting();
    void firstPublishedSample();
    void intervalPreservesHistory();
};

void MonitorTest::cpuAccounting()
{
    const auto cpu = ProcReader::parseCpu("cpu 100 20 30 800 40 5 3 2 50 10");
    QCOMPARE(cpu.total, quint64(1000));
    QCOMPARE(cpu.idle, quint64(840));
    QCOMPARE(ProcReader::cpuUsage({ 2000, 1700 }, { 1000, 850 }), 15.0);
    QCOMPARE(ProcReader::cpuUsage({ 1000, 800 }, { 1000, 800 }), 0.0);
    QCOMPARE(ProcReader::cpuUsage({ 10, 5 }, { 1000, 800 }), 0.0);
    QCOMPARE(ProcReader::cpuUsage({ 1100, 790 }, { 1000, 800 }), 100.0);
}

void MonitorTest::memoryAccounting()
{
    const auto memory = ProcReader::parseMemory("MemTotal: 1000 kB\nMemAvailable: 600 kB\n"
                                               "MemFree: 100 kB\nCached: 200 kB\nSReclaimable: 50 kB\n"
                                               "SwapTotal: 500 kB\nSwapFree: 300 kB\n");
    QCOMPARE(memory.value(QStringLiteral("memoryUsed")).toULongLong(), quint64(400 * 1024));
    QCOMPARE(memory.value(QStringLiteral("memoryPercent")).toDouble(), 40.0);
    QCOMPARE(memory.value(QStringLiteral("swapUsed")).toULongLong(), quint64(200 * 1024));
    const auto fallback = ProcReader::parseMemory("MemTotal: 1000 kB\nMemFree: 100 kB\nBuffers: 50 kB\n"
                                                 "Cached: 200 kB\nSReclaimable: 50 kB\nShmem: 30 kB\n");
    QCOMPARE(fallback.value(QStringLiteral("memoryAvailable")).toULongLong(), quint64(370 * 1024));
    QCOMPARE(ProcReader::parseMemory({}).value(QStringLiteral("memoryPercent")).toDouble(), 0.0);
    QCOMPARE(ProcReader::parseMemory("MemTotal: 100 kB\nMemAvailable: 200 kB\n")
             .value(QStringLiteral("memoryUsed")).toULongLong(), quint64(0));
}

void MonitorTest::processNamesAndFields()
{
    ProcessRecord process;
    QVERIFY(ProcReader::parseProcess(processStat(123, QStringLiteral("worker (pool) 3"), 25, 999), &process, 4096));
    QCOMPARE(process.pid, 123);
    QCOMPARE(process.parentPid, 42);
    QCOMPARE(process.name, QStringLiteral("worker (pool) 3"));
    QCOMPARE(process.cpuTicks, quint64(25));
    QCOMPARE(process.startTicks, quint64(999));
    QCOMPARE(process.memory, quint64(7 * 4096));
    QCOMPARE(process.threads, 3);
    QCOMPARE(process.nice, 5);
    QVERIFY(!process.kernelThread);
    QVERIFY(ProcReader::parseProcess(processStat(2, QStringLiteral("kthreadd"), 0, 1, 0x00208040), &process, 4096));
    QVERIFY(process.kernelThread);
    QVERIFY(!ProcReader::parseProcess("123 (short) R 1", &process, 4096));
    QVERIFY(!ProcReader::parseProcess("broken", &process, 4096));

    uint uid = 0;
    QVERIFY(ProcReader::parseUid("Name:\tagent\nUmask:\t0077\nUid:\t1000\t1001\t1000\t1001\n", &uid));
    QCOMPARE(uid, 1001u);
    QVERIFY(!ProcReader::parseUid("Name:\tagent\n", &uid));

    QCOMPARE(ProcReader::applicationProgram(commandLine({ "/usr/lib/chromium/chromium", "--type=renderer" })),
             QStringLiteral("/usr/lib/chromium/chromium"));
    QCOMPARE(ProcReader::applicationProgram(commandLine({ "/usr/bin/python3", "-u", "/usr/bin/fixture-app", "--flag" })),
             QStringLiteral("/usr/bin/fixture-app"));
    QCOMPARE(ProcReader::applicationProgram(commandLine({ "sh", "-c", "fixture-app" })), QString());
}

void MonitorTest::samplingAndPidReuse()
{
    QTemporaryDir directory;
    const QString proc = directory.path() + QStringLiteral("/proc");
    const QString sys = directory.path() + QStringLiteral("/sys");
    QVERIFY(writeFile(proc + QStringLiteral("/stat"), "cpu 100 0 50 850 0 0 0 0\ncpu0 100 0 50 850 0 0 0 0\n"));
    QVERIFY(writeFile(proc + QStringLiteral("/meminfo"), "MemTotal: 1000 kB\nMemAvailable: 600 kB\n"));
    QVERIFY(writeFile(proc + QStringLiteral("/123/stat"), processStat(123, QStringLiteral("sample"), 20, 1000)));
    QVERIFY(QFile::link(QStringLiteral("/usr/bin/fixture-tool (deleted)"), proc + QStringLiteral("/123/exe")));
    ProcReader reader(proc, sys);
    const auto first = reader.sample();
    QVERIFY(first.available);
    QVERIFY(first.baseline);
    QCOMPARE(first.processes.size(), 1);
    QCOMPARE(first.processes[0].uid, getuid());
    QVERIFY(!first.processes[0].kernelThread);
    QCOMPARE(first.processes[0].cpu, 0.0);
    QCOMPARE(first.processes[0].displayName, QStringLiteral("fixture-tool"));
    QVERIFY(!first.processes[0].ioAvailable);

    QVERIFY(writeFile(proc + QStringLiteral("/stat"), "cpu 200 0 100 1700 0 0 0 0\ncpu0 200 0 100 1700 0 0 0 0\n"));
    QVERIFY(writeFile(proc + QStringLiteral("/123/stat"), processStat(123, QStringLiteral("sample"), 70, 1000)));
    const auto second = reader.sample();
    QVERIFY(!second.baseline);
    QCOMPARE(second.metrics.value(QStringLiteral("cpuPercent")).toDouble(), 15.0);
    QCOMPARE(second.processes[0].cpu, 5.0);

    QVERIFY(writeFile(proc + QStringLiteral("/stat"), "cpu 300 0 150 2550 0 0 0 0\n"));
    QVERIFY(writeFile(proc + QStringLiteral("/123/stat"), processStat(123, QStringLiteral("replacement"), 9999, 2000)));
    const auto reused = reader.sample();
    QCOMPARE(reused.processes[0].cpu, 0.0);
    QCOMPARE(reused.processes[0].name, QStringLiteral("replacement"));
}

void MonitorTest::deviceAccounting()
{
    QTemporaryDir directory;
    const QString proc = directory.path() + QStringLiteral("/proc");
    const QString sys = directory.path() + QStringLiteral("/sys");
    QVERIFY(writeFile(proc + QStringLiteral("/stat"), "cpu 1 0 1 98 0 0 0 0\n"));
    QVERIFY(writeFile(proc + QStringLiteral("/meminfo"), "MemTotal: 1000 kB\n"));
    QVERIFY(QDir().mkpath(sys + QStringLiteral("/class/net/test0/device")));
    QVERIFY(QDir().mkpath(sys + QStringLiteral("/block/sda/device")));
    const QByteArray network = "test0: 1000 0 0 0 0 0 0 0 2000 0 0 0 0 0 0 0\n"
                              "lo: 5000 0 0 0 0 0 0 0 5000 0 0 0 0 0 0 0\n"
                              "veth0: 9000 0 0 0 0 0 0 0 9000 0 0 0 0 0 0 0\n";
    QVERIFY(writeFile(proc + QStringLiteral("/net/dev"), network));
    QVERIFY(writeFile(proc + QStringLiteral("/diskstats"),
                      "8 0 sda 0 0 100 0 0 0 200 0 0 0 0\n8 1 sda1 0 0 100 0 0 0 200 0 0 0 0\n"));
    ProcReader reader(proc, sys);
    const auto first = reader.sample();
    QCOMPARE(first.metrics.value(QStringLiteral("received")).toULongLong(), quint64(1000));
    QCOMPARE(first.metrics.value(QStringLiteral("interfaces")).toList().size(), 2);
    QCOMPARE(first.metrics.value(QStringLiteral("disks")).toList().size(), 1);
    QCOMPARE(first.metrics.value(QStringLiteral("diskRead")).toDouble(), 0.0);
    QTest::qWait(20);
    QVERIFY(writeFile(proc + QStringLiteral("/net/dev"), "test0: 3000 0 0 0 0 0 0 0 4000 0 0 0 0 0 0 0\n"));
    QVERIFY(writeFile(proc + QStringLiteral("/diskstats"), "8 0 sda 0 0 200 0 0 0 400 0 0 0 0\n"));
    const auto second = reader.sample();
    QVERIFY(second.metrics.value(QStringLiteral("download")).toDouble() > 0);
    QVERIFY(second.metrics.value(QStringLiteral("diskRead")).toDouble() > 0);
    QVERIFY(writeFile(proc + QStringLiteral("/net/dev"), "test0: 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"));
    QCOMPARE(reader.sample().metrics.value(QStringLiteral("download")).toDouble(), 0.0);
}

void MonitorTest::unavailableProc()
{
    QTemporaryDir directory;
    ProcReader reader(directory.path(), directory.path());
    const auto snapshot = reader.sample();
    QVERIFY(!snapshot.available);
    QVERIFY(!snapshot.error.isEmpty());
}

void MonitorTest::modelUpdatesPreserveIdentity()
{
    ProcessModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
    model.apply({ record(10, QStringLiteral("first")), record(20, QStringLiteral("second")) });
    QPersistentModelIndex retained(model.index(1));
    model.apply({ record(5, QStringLiteral("new")), record(20, QStringLiteral("second"), 30) });
    QVERIFY(retained.isValid());
    QCOMPARE(retained.data(ProcessModel::PidRole).toInt(), 20);
    QCOMPARE(retained.data(ProcessModel::CpuRole).toDouble(), 30.0);
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.find(10), nullptr);
    QCOMPARE(model.find(20)->name, QStringLiteral("second"));
    model.apply({});
    QVERIFY(!retained.isValid());
}

void MonitorTest::filteringAndSorting()
{
    ProcessModel model;
    ProcessFilter filter(&model);
    QAbstractItemModelTester tester(&filter, QAbstractItemModelTester::FailureReportingMode::QtTest);
    auto foreign = record(30, QStringLiteral("foreign"), 90);
    foreign.uid = getuid() + 1;
    model.apply({ record(10, QStringLiteral("Terminal"), 10), record(20, QStringLiteral("Browser"), 30), foreign });
    QCOMPARE(filter.count(), 2);
    QCOMPARE(filter.get(0).value(QStringLiteral("pid")).toInt(), 20);
    filter.setSearch(QStringLiteral("terminal"));
    QCOMPARE(filter.count(), 1);
    filter.setSearch(QStringLiteral("--example"));
    QCOMPARE(filter.count(), 2);
    filter.setSearch(QStringLiteral("20"));
    QCOMPARE(filter.count(), 1);
    filter.setSearch({});
    filter.setAllUsers(true);
    QCOMPARE(filter.count(), 3);
    QCOMPARE(filter.get(0).value(QStringLiteral("pid")).toInt(), 30);
    filter.sortBy(QStringLiteral("pid"));
    QCOMPARE(filter.get(0).value(QStringLiteral("pid")).toInt(), 10);
    filter.sortBy(QStringLiteral("pid"));
    QCOMPARE(filter.get(0).value(QStringLiteral("pid")).toInt(), 30);
    QCOMPARE(filter.indexOf(20), 1);
    QVERIFY(filter.get(100).isEmpty());
}

void MonitorTest::terminateChild_data()
{
    QTest::addColumn<bool>("force");
    QTest::newRow("graceful") << false;
    QTest::newRow("force") << true;
}

void MonitorTest::terminateChild()
{
    QFETCH(bool, force);
    QProcess child;
    child.start(QStringLiteral("/usr/bin/sleep"), { QStringLiteral("30") });
    QVERIFY(child.waitForStarted());
    const int pid = child.processId();
    QFile stat(QStringLiteral("/proc/%1/stat").arg(pid));
    QVERIFY(stat.open(QIODevice::ReadOnly));
    ProcessRecord process;
    QVERIFY(ProcReader::parseProcess(stat.readAll(), &process, 1));
    QVERIFY2(Monitor::signalProcess(pid, process.startTicks, force).isEmpty(), "Unable to signal the test child");
    QVERIFY(child.waitForFinished(3000));
}

void MonitorTest::rejectReusedAndProtectedPid()
{
    QVERIFY(!Monitor::signalProcess(1, 0, false).isEmpty());
    QVERIFY(!Monitor::signalProcess(getpid(), 0, false).isEmpty());
    QProcess child;
    child.start(QStringLiteral("/usr/bin/sleep"), { QStringLiteral("30") });
    QVERIFY(child.waitForStarted());
    QVERIFY(!Monitor::signalProcess(child.processId(), 0, true).isEmpty());
    QCOMPARE(child.state(), QProcess::Running);
    child.terminate();
    QVERIFY(child.waitForFinished(3000));
}

void MonitorTest::rejectForeignProcess()
{
    // kthreadd belongs to root; the kernel refuses the signal, not the monitor.
    QFile stat(QStringLiteral("/proc/2/stat"));
    if (!stat.open(QIODevice::ReadOnly) || getuid() == 0)
        QSKIP("Needs an unprivileged user and a visible kthreadd");
    ProcessRecord process;
    QVERIFY(ProcReader::parseProcess(stat.readAll(), &process, 1));
    QCOMPARE(Monitor::signalProcess(2, process.startTicks, false),
             QStringLiteral("You can only end your own processes."));
}

void MonitorTest::desktopIdentity()
{
    auto registry = ApplicationRegistry::instance();
    QTRY_VERIFY_WITH_TIMEOUT(!registry->loading(), 5000);
    const QString program = QCoreApplication::applicationFilePath();
    const QString name = QFileInfo(program).fileName();
    DesktopEntryData shortcut;
    shortcut.id = QStringLiteral("a-fixture-shortcut");
    shortcut.type = QStringLiteral("Application");
    shortcut.name = QStringLiteral("Fixture Shortcut");
    shortcut.exec = program + QStringLiteral(" --open-shortcut");
    DesktopEntryData application;
    application.id = name;
    application.type = QStringLiteral("Application");
    application.name = QStringLiteral("Fixture Application");
    application.icon = QStringLiteral("fixture-icon");
    application.exec = program;
    registry->applyScan({ shortcut, application });

    Monitor monitor;
    QTRY_VERIFY_WITH_TIMEOUT(monitor.processes()->indexOf(getpid()) >= 0, 5000);
    const auto self = monitor.processes()->get(monitor.processes()->indexOf(getpid()));
    QCOMPARE(self.value(QStringLiteral("name")).toString(), QStringLiteral("Fixture Application"));
    QCOMPARE(self.value(QStringLiteral("icon")).toString(), QStringLiteral("fixture-icon"));
    registry->startScan();
    QTRY_VERIFY_WITH_TIMEOUT(!registry->loading(), 5000);
}

void MonitorTest::scopesAndPss()
{
    QCOMPARE(ProcReader::scopeApplication("0::/user.slice/user-1000.slice/user@1000.service/app.slice/"
                                          "app-cutefish-cutefish\\x2dterminal-2091752.scope\n"),
             QStringLiteral("cutefish-terminal"));
    QCOMPARE(ProcReader::scopeApplication("0::/user.slice/app.slice/app-org.example.Viewer-12.scope"),
             QStringLiteral("org.example.Viewer"));
    QCOMPARE(ProcReader::scopeApplication("0::/app.slice/app-gnome-org.example.Editor@3a9f.service"),
             QStringLiteral("org.example.Editor"));
    QCOMPARE(ProcReader::scopeApplication("0::/app.slice/app-org.example.Player.service"),
             QStringLiteral("org.example.Player"));
    QCOMPARE(ProcReader::scopeApplication("0::/user.slice/user-1000.slice/session-580.scope"), QString());
    QCOMPARE(ProcReader::scopeApplication("0::/app.slice/xdg-desktop-portal-cutefish.service"), QString());
    QCOMPARE(ProcReader::scopeApplication("0::/app.slice/app-broken.scope"), QString());
    QCOMPARE(ProcReader::scopeApplication({}), QString());

    quint64 pss = 0;
    QVERIFY(ProcReader::parsePss("55d0-7ffd ---p 00000000 00:00 0 [rollup]\nRss: 161924 kB\n"
                                 "Pss: 99783 kB\nPss_Dirty: 42360 kB\n", &pss));
    QCOMPARE(pss, quint64(99783) * 1024);
    QVERIFY(!ProcReader::parsePss("Rss: 10 kB\n", &pss));
}

void MonitorTest::sampledScopesAndPss()
{
    QTemporaryDir directory;
    const QString proc = directory.path() + QStringLiteral("/proc");
    QVERIFY(writeFile(proc + QStringLiteral("/stat"), "cpu 100 0 50 850 0 0 0 0\n"));
    QVERIFY(writeFile(proc + QStringLiteral("/meminfo"), "MemTotal: 1000 kB\nMemAvailable: 600 kB\n"));
    QVERIFY(writeFile(proc + QStringLiteral("/100/stat"), processStat(100, QStringLiteral("viewer"), 0, 1000)));
    QVERIFY(writeFile(proc + QStringLiteral("/100/cgroup"), "0::/user.slice/user-1000.slice/session-2.scope\n"));
    QVERIFY(writeFile(proc + QStringLiteral("/100/smaps_rollup"), "Pss: 300 kB\n"));
    QVERIFY(writeFile(proc + QStringLiteral("/200/stat"), processStat(200, QStringLiteral("tool"), 0, 1000)));
    QVERIFY(writeFile(proc + QStringLiteral("/200/smaps_rollup"), "Pss: 50 kB\n"));
    ProcReader reader(proc, directory.path() + QStringLiteral("/sys"));
    auto snapshot = reader.sample();
    QCOMPARE(snapshot.processes.size(), 2);
    QVERIFY(snapshot.processes[0].scopeId.isEmpty());
    QVERIFY(!snapshot.processes[0].pssAvailable);
    QVERIFY(!snapshot.processes[1].pssAvailable);

    // The launcher adopts the process into its scope after the first read.
    QVERIFY(writeFile(proc + QStringLiteral("/100/cgroup"),
                      "0::/user.slice/app.slice/app-cutefish-org.example.Viewer-100.scope\n"));
    snapshot = reader.sample();
    QCOMPARE(snapshot.processes[0].scopeId, QStringLiteral("org.example.Viewer"));
    QVERIFY(snapshot.processes[0].pssAvailable);
    QCOMPARE(snapshot.processes[0].pss, quint64(300 * 1024));
    QVERIFY(!snapshot.processes[1].pssAvailable);

    // Settled processes keep their scope and, for a few seconds, their PSS;
    // members named by the caller and their new children are measured.
    QVERIFY(writeFile(proc + QStringLiteral("/100/cgroup"), "0::/elsewhere.scope\n"));
    QVERIFY(writeFile(proc + QStringLiteral("/100/smaps_rollup"), "Pss: 400 kB\n"));
    QByteArray child = processStat(300, QStringLiteral("helper"), 0, 1100);
    child.replace(" S 42 ", " S 200 ");
    QVERIFY(writeFile(proc + QStringLiteral("/300/stat"), child));
    QVERIFY(writeFile(proc + QStringLiteral("/300/smaps_rollup"), "Pss: 20 kB\n"));
    snapshot = reader.sample({ 200 });
    QCOMPARE(snapshot.processes.size(), 3);
    QCOMPARE(snapshot.processes[0].scopeId, QStringLiteral("org.example.Viewer"));
    QCOMPARE(snapshot.processes[0].pss, quint64(300 * 1024));
    QCOMPARE(snapshot.processes[1].pss, quint64(50 * 1024));
    QCOMPARE(snapshot.processes[2].parentPid, 200);
    QCOMPARE(snapshot.processes[2].pss, quint64(20 * 1024));
}

void MonitorTest::applicationGroups()
{
    auto registry = ApplicationRegistry::instance();
    QTRY_VERIFY_WITH_TIMEOUT(!registry->loading(), 5000);
    const QString program = QCoreApplication::applicationFilePath();
    DesktopEntryData application;
    application.id = QFileInfo(program).fileName();
    application.type = QStringLiteral("Application");
    application.name = QStringLiteral("Fixture Application");
    application.icon = QStringLiteral("fixture-icon");
    application.exec = program;
    DesktopEntryData standalone;
    standalone.id = QStringLiteral("fixture-standalone");
    standalone.type = QStringLiteral("Application");
    standalone.name = QStringLiteral("Fixture Standalone");
    standalone.exec = QStringLiteral("/usr/bin/sleep");
    DesktopEntryData hidden;
    hidden.id = QStringLiteral("fixture-hidden");
    hidden.type = QStringLiteral("Application");
    hidden.name = QStringLiteral("Fixture Hidden");
    hidden.exec = QStringLiteral("/usr/bin/cat");
    hidden.noDisplay = true;
    registry->applyScan({ application, standalone, hidden });

    QProcess helper;
    helper.start(QStringLiteral("/bin/sh"), { QStringLiteral("-c"), QStringLiteral("read line") });
    QProcess separate;
    separate.start(QStringLiteral("/usr/bin/sleep"), { QStringLiteral("30") });
    QProcess handler;
    handler.start(QStringLiteral("/usr/bin/cat"));
    QVERIFY(helper.waitForStarted() && separate.waitForStarted() && handler.waitForStarted());

    Monitor monitor;
    ProcessFilter *applications = monitor.applications();
    QTRY_VERIFY_WITH_TIMEOUT(applications->indexOf(getpid()) >= 0 && applications->indexOf(separate.processId()) >= 0, 5000);

    // The helper has no entry of its own and the handler only a hidden one,
    // so both join their parent; the second program has a listed entry and
    // stays apart even though this process started it.
    const auto self = applications->get(applications->indexOf(getpid()));
    QCOMPARE(self.value(QStringLiteral("name")).toString(), QStringLiteral("Fixture Application"));
    QCOMPARE(self.value(QStringLiteral("icon")).toString(), QStringLiteral("fixture-icon"));
    QCOMPARE(self.value(QStringLiteral("processCount")).toInt(), 3);
    QList<int> pids;
    QStringList names;
    double memory = 0;
    for (const QVariant &member : self.value(QStringLiteral("members")).toList()) {
        pids.append(member.toMap().value(QStringLiteral("pid")).toInt());
        names.append(member.toMap().value(QStringLiteral("name")).toString());
        memory += member.toMap().value(QStringLiteral("memory")).toDouble();
    }
    QVERIFY(names.contains(QFileInfo(program).fileName()));
    // Inline shell code names no program; the executable behind /bin/sh does.
    QVERIFY(names.contains(QFileInfo(QFileInfo(QStringLiteral("/bin/sh")).canonicalFilePath()).fileName()));
    QVERIFY(names.contains(QStringLiteral("cat")));
    QList<int> expected { int(getpid()), int(helper.processId()), int(handler.processId()) };
    std::sort(expected.begin(), expected.end());
    QCOMPARE(pids, expected);
    QCOMPARE(self.value(QStringLiteral("memory")).toDouble(), memory);
    QVERIFY(memory > 0);
    QVERIFY(!monitor.application(getpid(), self.value(QStringLiteral("startTicks")).toString())
            .value(QStringLiteral("canManage")).toBool());

    applications->setSearch(QString::number(helper.processId()));
    QCOMPARE(applications->count(), 1);
    applications->setSearch({});

    const auto other = applications->get(applications->indexOf(separate.processId()));
    QCOMPARE(other.value(QStringLiteral("name")).toString(), QStringLiteral("Fixture Standalone"));
    QCOMPARE(other.value(QStringLiteral("processCount")).toInt(), 1);
    const QString start = other.value(QStringLiteral("startTicks")).toString();
    QVERIFY(monitor.application(separate.processId(), start).value(QStringLiteral("canManage")).toBool());
    QVERIFY(monitor.endApplication(separate.processId(), QStringLiteral("1"), false).length() > 0);
    QCOMPARE(separate.state(), QProcess::Running);
    QCOMPARE(monitor.endApplication(separate.processId(), start, false), QString());
    QVERIFY(separate.waitForFinished(3000));

    helper.closeWriteChannel();
    handler.closeWriteChannel();
    QVERIFY(helper.waitForFinished(3000) && handler.waitForFinished(3000));
    registry->startScan();
    QTRY_VERIFY_WITH_TIMEOUT(!registry->loading(), 5000);
}

void MonitorTest::gpuAccounting()
{
    QTemporaryDir directory;
    const QString proc = directory.path() + QStringLiteral("/proc");
    const QString sys = directory.path() + QStringLiteral("/sys");
    QVERIFY(writeFile(proc + QStringLiteral("/stat"), "cpu 1 0 1 98 0 0 0 0\n"));
    QVERIFY(writeFile(proc + QStringLiteral("/meminfo"), "MemTotal: 1000 kB\n"));
    {
        ProcReader reader(proc, sys);
        QVERIFY(!reader.sample().metrics.value(QStringLiteral("gpuAvailable")).toBool());
    }

    const QString first = sys + QStringLiteral("/class/drm/card0/device");
    QVERIFY(writeFile(first + QStringLiteral("/gpu_busy_percent"), "37\n"));
    QVERIFY(writeFile(first + QStringLiteral("/mem_info_vram_used"), "1024\n"));
    QVERIFY(writeFile(first + QStringLiteral("/mem_info_vram_total"), "4096\n"));
    QVERIFY(writeFile(first + QStringLiteral("/product_name"), "Fixture Graphics\n"));
    QVERIFY(writeFile(first + QStringLiteral("/power/runtime_status"), "active\n"));
    QVERIFY(writeFile(first + QStringLiteral("/hwmon/hwmon4/temp1_input"), "41500\n"));
    QVERIFY(writeFile(first + QStringLiteral("/hwmon/hwmon4/freq1_input"), "800000000\n"));
    QVERIFY(QDir().mkpath(sys + QStringLiteral("/bus/pci/drivers/fixturegpu")));
    QVERIFY(QFile::link(sys + QStringLiteral("/bus/pci/drivers/fixturegpu"), first + QStringLiteral("/driver")));
    const QString second = sys + QStringLiteral("/class/drm/card1/device");
    QVERIFY(writeFile(second + QStringLiteral("/boot_vga"), "1\n"));
    QVERIFY(writeFile(second + QStringLiteral("/power/runtime_status"), "suspended\n"));
    QVERIFY(writeFile(second + QStringLiteral("/hwmon/hwmon5/temp1_input"), "60000\n"));
    QVERIFY(QDir().mkpath(sys + QStringLiteral("/class/drm/card0-eDP-1/device/hwmon/hwmon6")));
    QVERIFY(writeFile(sys + QStringLiteral("/class/drm/card0-eDP-1/device/gpu_busy_percent"), "99\n"));
    QVERIFY(writeFile(sys + QStringLiteral("/class/drm/card2/device/vendor"), "0x1234\n"));

    ProcReader reader(proc, sys);
    const auto metrics = reader.sample().metrics;
    const auto gpus = metrics.value(QStringLiteral("gpus")).toList();
    QCOMPARE(gpus.size(), 2);

    // The boot display GPU is listed first; it is suspended and is not read.
    const auto suspended = gpus[0].toMap();
    QVERIFY(!suspended.value(QStringLiteral("active")).toBool());
    QVERIFY(suspended.value(QStringLiteral("hasTemperature")).toBool());
    QCOMPARE(suspended.value(QStringLiteral("temperature")).toDouble(), -1.0);
    QVERIFY(!suspended.value(QStringLiteral("hasBusy")).toBool());

    const auto active = gpus[1].toMap();
    QCOMPARE(active.value(QStringLiteral("name")).toString(), QStringLiteral("Fixture Graphics"));
    QCOMPARE(active.value(QStringLiteral("driver")).toString(), QStringLiteral("fixturegpu"));
    QCOMPARE(active.value(QStringLiteral("busy")).toInt(), 37);
    QCOMPARE(active.value(QStringLiteral("temperature")).toDouble(), 41.5);
    QCOMPARE(active.value(QStringLiteral("frequency")).toDouble(), 800.0);
    QCOMPARE(active.value(QStringLiteral("vramUsed")).toULongLong(), quint64(1024));
    QCOMPARE(active.value(QStringLiteral("vramTotal")).toULongLong(), quint64(4096));

    // The overview prefers a GPU with an activity counter.
    QVERIFY(metrics.value(QStringLiteral("gpuAvailable")).toBool());
    QVERIFY(metrics.value(QStringLiteral("gpuHasBusy")).toBool());
    QCOMPARE(metrics.value(QStringLiteral("gpuName")).toString(), QStringLiteral("Fixture Graphics"));
    QCOMPARE(metrics.value(QStringLiteral("gpuPercent")).toDouble(), 37.0);
    QCOMPARE(metrics.value(QStringLiteral("gpuTemperature")).toDouble(), 41.5);
}

void MonitorTest::firstPublishedSample()
{
    Monitor monitor;
    int firstHistory = -1;
    connect(&monitor, &Monitor::updated, this, [&] {
        if (firstHistory < 0 && monitor.ready())
            firstHistory = monitor.history().value(QStringLiteral("timestamps")).toList().size();
    });
    QTRY_VERIFY_WITH_TIMEOUT(monitor.ready(), 5000);
    // The baseline sample only primes the counters and never reaches the history.
    QCOMPARE(firstHistory, 1);
    QVERIFY(monitor.metrics().value(QStringLiteral("memoryTotal")).toULongLong() > 0);
}

void MonitorTest::intervalPreservesHistory()
{
    Monitor monitor;
    QTRY_VERIFY_WITH_TIMEOUT(monitor.ready(), 5000);
    const auto history = monitor.history();
    QVERIFY(!history.value(QStringLiteral("cpuPercent")).toList().isEmpty());
    for (const int interval : { 2000, 5000, 1000, 5000 }) {
        monitor.setInterval(interval);
        QCOMPARE(monitor.interval(), interval);
        QCOMPARE(monitor.historyDuration(), 60);
    }
    monitor.setInterval(42);
    QCOMPARE(monitor.interval(), 5000);
    const auto timestamps = monitor.history().value(QStringLiteral("timestamps")).toList();
    QVERIFY(timestamps.size() >= history.value(QStringLiteral("timestamps")).toList().size());
    QCOMPARE(timestamps.first(), history.value(QStringLiteral("timestamps")).toList().first());
}

QTEST_GUILESS_MAIN(MonitorTest)

#include "tst_monitor.moc"
