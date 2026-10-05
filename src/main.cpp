// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cutefishapplication.h"
#include "historygraph.h"
#include "monitor.h"

#include <QClipboard>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>

int main(int argc, char *argv[])
{
    Cutefish::Application::prepare();
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("cutefish-system-monitor"));
    app.setApplicationVersion(QStringLiteral(SYSTEM_MONITOR_VERSION));
    app.setDesktopFileName(QStringLiteral("cutefish-system-monitor"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("utilities-system-monitor")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Cutefish System Monitor"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    auto bus = QDBusConnection::sessionBus();
    const QString service = QStringLiteral("com.cutefish.SystemMonitor");
    const QString path = QStringLiteral("/SystemMonitor");
    if (bus.isConnected() && !bus.registerService(service)) {
        QDBusInterface existing(service, path, service, bus);
        existing.setTimeout(1500);
        existing.call(QStringLiteral("activateWindow"));
        return 0;
    }

    qmlRegisterType<HistoryGraph>("Cutefish.SystemMonitor", 1, 0, "HistoryGraph");
    qmlRegisterUncreatableType<ProcessFilter>("Cutefish.SystemMonitor", 1, 0, "ProcessFilter",
                                            QStringLiteral("Provided by Monitor"));

    Monitor monitor;
    QObject::connect(&monitor, &Monitor::copyRequested, app.clipboard(), [](const QString &text) {
        QGuiApplication::clipboard()->setText(text);
    });
    bus.registerObject(path, &monitor, QDBusConnection::ExportAllSlots);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Monitor"), &monitor);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;

    auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    QObject::connect(&monitor, &Monitor::activationRequested, window, [window] {
        window->show();
        window->raise();
        window->requestActivate();
    });
    return app.exec();
}
