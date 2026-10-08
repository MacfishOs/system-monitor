// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "applicationregistry.h"
#include "cutefishapplication.h"
#include "desktopentry.h"
#include "historygraph.h"
#include "monitor.h"
#include "preferences.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QPainter>
#include <QProcess>
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
    void init();
    void resourcePages_data();
    void resourcePages();
    void keyboardAndSelection();
    void desktopContextMenu();
    void applicationView();
    void timestampedGraph();
    void measuredAlignment();
};

void UiTest::initTestCase()
{
    previousHandler = qInstallMessageHandler(captureWarnings);
    QStandardPaths::setTestModeEnabled(true);
}

void UiTest::cleanupTestCase()
{
    qInstallMessageHandler(previousHandler);
}

void UiTest::init()
{
    QFile::remove(Preferences::ownFile());
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
        for (int page = 0; page <= 5; ++page) {
            QVERIFY(window->setProperty("currentPage", page));
            QSignalSpy samples(&monitor, &Monitor::updated);
            QTRY_VERIFY_WITH_TIMEOUT(samples.count() > 0, 1500);
            QTest::qWait(80);
            if (page > 0) {
                auto chart = window->findChild<QQuickItem *>(QStringLiteral("resourceChart"));
                QVERIFY(chart);
                QVERIFY(chart->width() > 400);
                QCOMPARE(qRound(chart->mapToScene({}).x()), 252);
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

    auto scope = window->findChild<QQuickItem *>(QStringLiteral("processScope"));
    QVERIFY(scope && scope->setProperty("currentIndex", 1));
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

    auto scope = window->findChild<QQuickItem *>(QStringLiteral("processScope"));
    auto search = window->findChild<QQuickItem *>(QStringLiteral("processSearch"));
    auto list = window->findChild<QQuickItem *>(QStringLiteral("processList"));
    auto menu = window->findChild<QQuickWindow *>(QStringLiteral("processContextMenu"));
    QVERIFY(scope && search && list && menu);
    QVERIFY(scope->setProperty("currentIndex", 1));
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

void UiTest::applicationView()
{
    auto registry = ApplicationRegistry::instance();
    QTRY_VERIFY_WITH_TIMEOUT(!registry->loading(), 5000);
    const QString program = QCoreApplication::applicationFilePath();
    DesktopEntryData entry;
    entry.id = QFileInfo(program).fileName();
    entry.type = QStringLiteral("Application");
    entry.name = QStringLiteral("Fixture Application");
    entry.exec = program;
    registry->applyScan({ entry });
    QProcess helper;
    helper.start(QStringLiteral("/bin/sh"), { QStringLiteral("-c"), QStringLiteral("read line") });
    QVERIFY(helper.waitForStarted());

    Monitor monitor;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Monitor"), &monitor);
    QSignalSpy warnings(&engine, &QQmlEngine::warnings);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    QVERIFY(!engine.rootObjects().isEmpty());
    auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    QTRY_VERIFY_WITH_TIMEOUT(monitor.ready(), 5000);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    auto scope = window->findChild<QQuickItem *>(QStringLiteral("processScope"));
    auto search = window->findChild<QQuickItem *>(QStringLiteral("processSearch"));
    auto list = window->findChild<QQuickItem *>(QStringLiteral("processList"));
    auto menu = window->findChild<QQuickWindow *>(QStringLiteral("processContextMenu"));
    auto endButton = window->findChild<QQuickItem *>(QStringLiteral("endProcessButton"));
    QVERIFY(scope && search && list && menu && endButton);
    QCOMPARE(scope->property("currentIndex").toInt(), 1);
    QVERIFY(scope->setProperty("currentIndex", 0));
    QCOMPARE(monitor.scope(), 0);
    QCOMPARE(endButton->property("text").toString(), QStringLiteral("Quit"));
    search->setProperty("text", QStringLiteral("Fixture Application"));
    QTRY_COMPARE_WITH_TIMEOUT(monitor.applications()->count(), 1, 5000);
    QTest::qWait(50);
    QTest::mouseClick(window, Qt::RightButton, Qt::NoModifier, list->mapToScene(QPointF(80, 20)).toPoint());
    QTRY_VERIFY(menu->isVisible());
    QCOMPARE(window->property("selectedPid").toLongLong(), QCoreApplication::applicationPid());
    QVERIFY(window->property("selectedApplication").toBool());
    QVERIFY(QMetaObject::invokeMethod(menu, "close"));

    // The monitor's own application is never offered for quitting.
    QVERIFY(!window->property("selectedProcess").toMap().value(QStringLiteral("canManage")).toBool());
    QVERIFY(!endButton->isEnabled());
    QMetaObject::invokeMethod(window, "openDetails");
    auto details = window->findChild<QQuickWindow *>(QStringLiteral("processDetailsWindow"));
    QVERIFY(details);
    QTRY_VERIFY(details->isVisible());
    QCOMPARE(details->property("title").toString(), QStringLiteral("Application Details"));
    auto members = details->findChild<QQuickItem *>(QStringLiteral("applicationProcesses"));
    QVERIFY(members);
    QTRY_COMPARE(members->property("count").toInt(), 2);
    details->close();
    QCOMPARE(warnings.count(), 0);

    helper.closeWriteChannel();
    QVERIFY(helper.waitForFinished(3000));
    registry->startScan();
    QTRY_VERIFY_WITH_TIMEOUT(!registry->loading(), 5000);
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
    QCOMPARE(qRound(pageTitle->mapToScene({}).x()), 252);
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
    QQuickItem *minimizeButton = nullptr;
    for (auto item : window->findChildren<QQuickItem *>()) {
        if (item->window() == window && item->property("artwork").toString() == QStringLiteral("minimize"))
            minimizeButton = item;
    }
    QVERIFY(minimizeButton);
    const QRectF intervalRect = interval->mapRectToScene(interval->boundingRect());
    const QRectF minimizeRect = minimizeButton->mapRectToScene(minimizeButton->boundingRect());
    QCOMPARE(intervalRect.size(), minimizeRect.size());
    QCOMPARE(intervalRect.top(), minimizeRect.top());
    QCOMPARE(minimizeRect.left() - intervalRect.right(), window->property("windowButtonsSpacing").toReal());
    auto table = window->findChild<QQuickItem *>(QStringLiteral("processTable"));
    auto scope = window->findChild<QQuickItem *>(QStringLiteral("processScope"));
    QVERIFY(table && scope);
    const QRectF actionRect = endButton->mapRectToScene(endButton->boundingRect());
    const QRectF tableRect = table->mapRectToScene(table->boundingRect());
    QCOMPARE(actionRect.height(), search->height());
    QVERIFY(qAbs(actionRect.center().y() - searchCenter) < 0.01);
    QCOMPARE(actionRect.right(), tableRect.right());
    QCOMPARE(actionRect.width(), actionRect.height());
    auto scopeList = scope->property("contentItem").value<QQuickItem *>();
    QVERIFY(scopeList);
    auto scopeHighlight = scopeList->property("highlightItem").value<QQuickItem *>();
    QVERIFY(scopeHighlight && !scopeHighlight->childItems().isEmpty());
    auto selectedPill = scopeHighlight->childItems().first();
    const QRectF selectedRect = selectedPill->mapRectToScene(selectedPill->boundingRect());
    QVERIFY(qAbs(selectedRect.center().y() - searchCenter) < 0.01);
    auto actionIcon = window->findChild<QQuickItem *>(QStringLiteral("endProcessIcon"));
    QVERIFY(actionIcon);
    const QRectF iconRect = actionIcon->mapRectToScene(actionIcon->boundingRect());
    QCOMPARE(iconRect.size(), QSizeF(16, 16));
    QVERIFY(qAbs(iconRect.center().x() - actionRect.center().x()) < 0.01);
    QVERIFY(qAbs(iconRect.center().y() - searchCenter) < 0.01);
    auto scrollBar = window->findChild<QQuickItem *>(QStringLiteral("processScrollBar"));
    QVERIFY(scrollBar);
    const QRectF scrollBarRect = scrollBar->mapRectToScene(scrollBar->boundingRect());
    const QRectF listRect = list->mapRectToScene(list->boundingRect());
    QCOMPARE(scrollBarRect.right(), window->width());
    QCOMPARE(scrollBarRect.top(), listRect.top());
    QCOMPARE(scrollBarRect.bottom(), listRect.bottom());
    qInfo() << "Process action rect:" << actionRect << "icon rect:" << iconRect
            << "selected scope rect:" << selectedRect
            << "scroll bar rect:" << scrollBarRect << "list rect:" << listRect
            << "table rect:" << tableRect
            << "Interval button rect:" << intervalRect << "minimize button rect:" << minimizeRect
            << "Process list height:" << list->height();
    QTRY_VERIFY_WITH_TIMEOUT(monitor.ready(), 5000);
    for (const QString &name : { QStringLiteral("cpuSummary"), QStringLiteral("memorySummary"),
                               QStringLiteral("networkSummary"), QStringLiteral("diskSummary"),
                               QStringLiteral("gpuSummary") }) {
        auto summary = window->findChild<QQuickItem *>(name);
        QVERIFY(summary);
        if (!summary->isVisible())
            continue;
        QCOMPARE(qRound(summary->mapToScene({}).x()), 12);
        QCOMPARE(qRound(summary->width()), 216);
        QCOMPARE(qRound(summary->height()), 96);
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
