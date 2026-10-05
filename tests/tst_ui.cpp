// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cutefishapplication.h"
#include "historygraph.h"
#include "monitor.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QPainter>
#include <QtTest>

static QStringList layoutWarnings;
static QtMessageHandler previousHandler = nullptr;

static void captureWarnings(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    if (message.contains(QLatin1String("recursive rearrange"))
            || message.contains(QLatin1String("Binding loop"))
            || message.contains(QLatin1String("Unable to assign")))
        layoutWarnings.append(message);
    if (previousHandler)
        previousHandler(type, context, message);
}

class UiTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void resourcePages_data();
    void resourcePages();
    void keyboardAndSelection();
    void desktopContextMenu();
    void timestampedGraph();
    void measuredAlignment();
};

void UiTest::initTestCase()
{
    previousHandler = qInstallMessageHandler(captureWarnings);
}

void UiTest::cleanupTestCase()
{
    qInstallMessageHandler(previousHandler);
}

void UiTest::resourcePages_data()
{
    QTest::addColumn<bool>("dark");
    QTest::newRow("light") << false;
    QTest::newRow("dark") << true;
}

void UiTest::resourcePages()
{
    QFETCH(bool, dark);
    Monitor monitor;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Monitor"), &monitor);
    QQmlComponent theme(&engine);
    theme.setData(QByteArray("import QtQuick\nimport FishUI as FishUI\nQtObject { Component.onCompleted: FishUI.Theme.darkMode = ")
                  + (dark ? "true" : "false") + " }", QUrl());
    std::unique_ptr<QObject> themeOverride(theme.create());
    QVERIFY2(themeOverride != nullptr, qPrintable(theme.errorString()));

    QSignalSpy warnings(&engine, &QQmlEngine::warnings);
    layoutWarnings.clear();
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    QVERIFY(!engine.rootObjects().isEmpty());
    auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    QVERIFY(window);
    QTRY_VERIFY_WITH_TIMEOUT(monitor.ready(), 5000);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    for (const QSize &size : { QSize(1040, 700), QSize(840, 600) }) {
        window->resize(size);
        for (int page = 0; page <= 4; ++page) {
            QVERIFY(window->setProperty("currentPage", page));
            QSignalSpy samples(&monitor, &Monitor::updated);
            QTRY_VERIFY_WITH_TIMEOUT(samples.count() > 0, 1500);
            QTest::qWait(80);
            if (page > 0) {
                auto chart = window->findChild<QQuickItem *>(QStringLiteral("resourceChart"));
                QVERIFY(chart);
                QVERIFY(chart->width() > 400);
                QCOMPARE(qRound(chart->mapToScene({}).x()), 264);
                auto graph = chart->findChild<HistoryGraph *>(QStringLiteral("historyGraph"));
                auto scale = chart->findChild<QQuickItem *>(QStringLiteral("chartScaleLabel"));
                QVERIFY(graph && scale);
                const qreal scaleBottom = scale->mapToScene(QPointF(0, scale->height())).y();
                const qreal plotTop = graph->mapToScene({}).y();
                QVERIFY(plotTop - scaleBottom >= 9.5);
                if (page == 1) {
                    graph->setValues({ 100.0, 100.0, 100.0 });
                    graph->setTimestamps({ 0, 30000, 60000 });
                    QTest::qWait(30);
                    QCOMPARE(graph->effectiveMaximum(), 100.0);
                    qInfo() << "Chart scale gap:" << plotTop - scaleBottom;
                    qInfo() << "Chart right inset:"
                            << window->width() - chart->mapToScene(QPointF(chart->width(), 0)).x();
                }
            }
            const QString context = QStringLiteral("Page %1 at %2 x %3: %4")
                    .arg(page).arg(size.width()).arg(size.height()).arg(layoutWarnings.join(QLatin1Char('\n')));
            QVERIFY2(layoutWarnings.isEmpty(), qPrintable(context));
            QCOMPARE(warnings.count(), 0);
        }
    }
}

void UiTest::keyboardAndSelection()
{
    Monitor monitor;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Monitor"), &monitor);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    QVERIFY(!engine.rootObjects().isEmpty());
    auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    QTRY_VERIFY_WITH_TIMEOUT(monitor.ready(), 5000);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->requestActivate();
    QTest::qWait(50);

    auto search = window->findChild<QQuickItem *>(QStringLiteral("processSearch"));
    QVERIFY(search);
    QTest::keyClick(window, Qt::Key_F, Qt::ControlModifier);
    QTRY_VERIFY(search->hasActiveFocus());
    search->setProperty("text", QString::number(QCoreApplication::applicationPid()));
    QTRY_VERIFY(monitor.processes()->count() > 0);
    const QVariantMap selected = monitor.processes()->get(0);
    QVERIFY(window->setProperty("selectedPid", selected.value(QStringLiteral("pid"))));
    QVERIFY(window->setProperty("selectedStart", selected.value(QStringLiteral("startTicks"))));
    monitor.processes()->sortBy(QStringLiteral("memory"));
    QCOMPARE(window->property("selectedPid").toInt(), selected.value(QStringLiteral("pid")).toInt());
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_VERIFY(search->property("text").toString().isEmpty());
    search->setProperty("text", QString::number(QCoreApplication::applicationPid()));
    auto clearIcon = window->findChild<QQuickItem *>(QStringLiteral("processSearchClearIcon"));
    QVERIFY(clearIcon);
    QTRY_VERIFY(clearIcon->isVisible());
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                     clearIcon->mapToScene(QPointF(clearIcon->width() / 2, clearIcon->height() / 2)).toPoint());
    QTRY_VERIFY(search->property("text").toString().isEmpty());

    QVERIFY(window->setProperty("selectedPid", selected.value(QStringLiteral("pid"))));
    QVERIFY(window->setProperty("selectedStart", selected.value(QStringLiteral("startTicks"))));
    QMetaObject::invokeMethod(window, "openDetails");
    auto details = window->findChild<QQuickWindow *>(QStringLiteral("processDetailsWindow"));
    QVERIFY(details);
    QTRY_VERIFY(details->isVisible());
    QVERIFY(!details->property("blurEnabled").toBool());
    QCOMPARE(details->property("process").toMap().value(QStringLiteral("pid")).toInt(), selected.value(QStringLiteral("pid")).toInt());
    details->close();
}

void UiTest::desktopContextMenu()
{
    Monitor monitor;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Monitor"), &monitor);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    QVERIFY(!engine.rootObjects().isEmpty());
    auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    QTRY_VERIFY_WITH_TIMEOUT(monitor.ready(), 5000);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    auto search = window->findChild<QQuickItem *>(QStringLiteral("processSearch"));
    auto list = window->findChild<QQuickItem *>(QStringLiteral("processList"));
    auto menu = window->findChild<QQuickWindow *>(QStringLiteral("processContextMenu"));
    QVERIFY(search && list && menu);
    search->setProperty("text", QString::number(QCoreApplication::applicationPid()));
    QTRY_VERIFY(monitor.processes()->count() > 0);
    QTest::qWait(50);
    QTest::mouseClick(window, Qt::RightButton, Qt::NoModifier,
                     list->mapToScene(QPointF(80, 20)).toPoint());
    QTRY_VERIFY(menu->isVisible());
    QCOMPARE(window->property("selectedPid").toLongLong(), QCoreApplication::applicationPid());
    QSignalSpy copies(&monitor, &Monitor::copyRequested);
    QVERIFY(QMetaObject::invokeMethod(menu, "setCurrentIndex", Q_ARG(QVariant, 1)));
    QVERIFY(QMetaObject::invokeMethod(menu, "activateCurrentRow"));
    QTRY_COMPARE(copies.count(), 1);
    QCOMPARE(copies.first().first().toString(), QString::number(QCoreApplication::applicationPid()));
    QTRY_VERIFY(!menu->isVisible());
}

void UiTest::timestampedGraph()
{
    HistoryGraph graph;
    graph.setWidth(202);
    graph.setHeight(104);
    graph.setColor(Qt::red);
    graph.setGridColor(Qt::transparent);
    graph.setDuration(10000);
    graph.setValues({ 0.0, 0.0, 100.0, 0.0 });
    graph.setTimestamps({ 0, 1000, 3000, 10000 });

    QImage image(202, 104, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    graph.paint(&painter);
    painter.end();

    int peakLeft = image.width();
    int peakRight = -1;
    for (int y = 0; y < 6; ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor pixel = image.pixelColor(x, y);
            if (pixel.red() > 200 && pixel.alpha() > 150) {
                peakLeft = qMin(peakLeft, x);
                peakRight = qMax(peakRight, x);
            }
        }
    }
    QVERIFY(peakRight >= peakLeft);
    // The peak occurred seven seconds ago in a ten-second window.
    QVERIFY(qAbs((peakLeft + peakRight) / 2.0 - 61.0) < 5);
}

void UiTest::measuredAlignment()
{
    Monitor monitor;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Monitor"), &monitor);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    QVERIFY(!engine.rootObjects().isEmpty());
    auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QTest::qWait(100);

    auto applicationTitle = window->findChild<QQuickItem *>(QStringLiteral("applicationTitle"));
    auto pageTitle = window->findChild<QQuickItem *>(QStringLiteral("pageTitle"));
    QVERIFY(applicationTitle && pageTitle);
    QVERIFY(!window->findChild<QQuickItem *>(QStringLiteral("pauseButton")));
    QVERIFY(!window->property("blurEnabled").toBool());
    const qreal firstBaseline = applicationTitle->mapToScene({}).y() + applicationTitle->property("baselineOffset").toReal();
    const qreal secondBaseline = pageTitle->mapToScene({}).y() + pageTitle->property("baselineOffset").toReal();
    QVERIFY(qAbs(firstBaseline - secondBaseline) < 0.01);
    QCOMPARE(qRound(pageTitle->mapToScene({}).x()), 264);
    const qreal capitalCenter = applicationTitle->property("capCenter").toReal();
    const qreal buttonCenter = window->property("windowButtonsTopMargin").toReal() + 16;
    QVERIFY(qAbs(buttonCenter - capitalCenter) <= 0.5);

    auto search = window->findChild<QQuickItem *>(QStringLiteral("processSearch"));
    auto searchBackground = window->findChild<QQuickItem *>(QStringLiteral("processSearchBackground"));
    auto searchIcon = window->findChild<QQuickItem *>(QStringLiteral("processSearchIcon"));
    auto clearIcon = window->findChild<QQuickItem *>(QStringLiteral("processSearchClearIcon"));
    QVERIFY(search && searchBackground && searchIcon && clearIcon);
    QCOMPARE(searchBackground->property("radius").toReal(), search->height() / 2);
    const qreal searchCenter = search->mapToScene(QPointF(0, search->height() / 2)).y();
    const qreal iconCenter = searchIcon->mapToScene(QPointF(0, searchIcon->height() / 2)).y();
    QVERIFY(qAbs(searchCenter - iconCenter) < 0.01);
    const qreal clearCenter = clearIcon->mapToScene(QPointF(0, clearIcon->height() / 2)).y();
    QVERIFY(qAbs(searchCenter - clearCenter) < 0.01);
    QCOMPARE(clearIcon->size(), searchIcon->size());
    QCOMPARE(searchIcon->x(), search->width() - clearIcon->x() - clearIcon->width());

    qInfo().noquote() << QStringLiteral("Header baselines: %1, %2; title left: %3; button centre: %4; capital centre: %5; search centre: %6; icon centre: %7; clear centre: %8")
                         .arg(firstBaseline).arg(secondBaseline).arg(pageTitle->mapToScene({}).x())
                         .arg(buttonCenter).arg(capitalCenter).arg(searchCenter).arg(iconCenter).arg(clearCenter);
    auto endButton = window->findChild<QQuickItem *>(QStringLiteral("endProcessButton"));
    auto interval = window->findChild<QQuickItem *>(QStringLiteral("refreshInterval"));
    auto list = window->findChild<QQuickItem *>(QStringLiteral("processList"));
    QVERIFY(endButton && interval && list);
    const qreal actionInset = window->height() - endButton->mapToScene(QPointF(0, endButton->height())).y();
    const qreal intervalInset = window->height() - interval->mapToScene(QPointF(0, interval->height())).y();
    const qreal actionRightInset = window->width() - endButton->mapToScene(QPointF(endButton->width(), 0)).x();
    qInfo() << "Process action bottom inset:" << actionInset
            << "Sidebar interval bottom inset:" << intervalInset
            << "Process action right inset:" << actionRightInset
            << "Process list height:" << list->height();
    for (const QString &name : { QStringLiteral("cpuSummary"), QStringLiteral("memorySummary"),
                               QStringLiteral("networkSummary"), QStringLiteral("diskSummary") }) {
        auto summary = window->findChild<QQuickItem *>(name);
        QVERIFY(summary);
        QCOMPARE(qRound(summary->mapToScene({}).x()), 12);
        QCOMPARE(qRound(summary->width()), 216);
        QCOMPARE(qRound(summary->height()), 112);
    }
}

int main(int argc, char **argv)
{
    Cutefish::Application::prepare();
    QGuiApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("cutefish-system-monitor-test"));
    qmlRegisterType<HistoryGraph>("Cutefish.SystemMonitor", 1, 0, "HistoryGraph");
    qmlRegisterUncreatableType<ProcessFilter>("Cutefish.SystemMonitor", 1, 0, "ProcessFilter", QStringLiteral("Provided by Monitor"));
    UiTest test;
    const int result = QTest::qExec(&test, argc, argv);
    return result;
}

#include "tst_ui.moc"
