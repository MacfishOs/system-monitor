// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FishUI as FishUI
import Cutefish.SystemMonitor

FishUI.Window {
    id: rootWindow
    objectName: "monitorWindow"
    title: qsTr("System Monitor")
    width: 1040
    height: 700
    minimumWidth: 840
    minimumHeight: 600
    contentTopMargin: 0
    header.height: 48
    windowButtonsTopMargin: applicationTitle.capCenter - header.buttonSize / 2
    blurEnabled: false
    background.color: FishUI.Theme.backgroundColor
    background.opacity: 1

    property int currentPage: 0
    property int selectedPid: 0
    property string selectedStart: ""
    readonly property int sidebarWidth: 240
    readonly property var pageTitles: [qsTr("Processes"), qsTr("CPU"), qsTr("Memory"), qsTr("Network"), qsTr("Disk")]
    readonly property var selectedProcess: {
        Monitor.metrics;
        return Monitor.process(selectedPid, selectedStart);
    }

    LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    Rectangle {
        objectName: "sidebarBackground"
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: rootWindow.sidebarWidth
        color: FishUI.Theme.darkMode ? Qt.lighter(FishUI.Theme.backgroundColor, 1.5)
                                     : Qt.darker(FishUI.Theme.backgroundColor, 1.05)
        opacity: 0.7
    }

    headerItem: Item {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 12
            spacing: 12

            FishUI.AlignedLabel {
                id: applicationTitle
                objectName: "applicationTitle"
                Layout.preferredWidth: rootWindow.sidebarWidth - 8
                textBaseline: 28
                text: rootWindow.title
                font.pointSize: 11
                color: rootWindow.active ? FishUI.Theme.textColor : FishUI.Theme.disabledTextColor
                elide: Text.ElideRight
            }

            FishUI.AlignedLabel {
                objectName: "pageTitle"
                Layout.fillWidth: true
                textBaseline: 28
                text: rootWindow.pageTitles[rootWindow.currentPage]
                font.pointSize: 11
                color: rootWindow.active ? FishUI.Theme.textColor : FishUI.Theme.disabledTextColor
            }
        }
    }

    Flickable {
        id: sidebar
        objectName: "resourceSidebar"
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.topMargin: rootWindow.header.height
        anchors.bottom: footer.top
        width: rootWindow.sidebarWidth
        contentHeight: sidebarContent.height + 24
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        FishUI.WheelHandler {
            target: sidebar
        }
        FishUI.ScrollBar.vertical: FishUI.ScrollBar {}

        Column {
            id: sidebarContent
            x: 12
            y: 8
            width: parent.width - 24
            spacing: 12

            ItemDelegate {
                id: processesButton
                objectName: "processesNavigation"
                width: parent.width
                height: 36
                highlighted: rootWindow.currentPage === 0
                onClicked: rootWindow.currentPage = 0
                Accessible.name: qsTr("Processes")

                background: FishUI.RoundedRectangle {
                    radius: FishUI.Theme.smallRadius
                    color: processesButton.highlighted ? Qt.alpha(FishUI.Theme.highlightColor, 0.12) : processesButton.hovered ? Qt.alpha(FishUI.Theme.textColor, 0.05) : "transparent"
                }

                contentItem: RowLayout {
                    spacing: 10
                    FishUI.Icon {
                        Layout.preferredWidth: 18
                        Layout.preferredHeight: 18
                        name: "list"
                        color: processesButton.highlighted ? FishUI.Theme.highlightColor : FishUI.Theme.textColor
                    }
                    FishUI.Label {
                        Layout.fillWidth: true
                        text: qsTr("Processes")
                        font.weight: Font.Medium
                    }
                    FishUI.Label {
                        text: Monitor.ready ? Monitor.metrics.processCount : ""
                        color: FishUI.Theme.disabledTextColor
                        font.pointSize: 9
                    }
                }
            }

            ResourceSummary {
                objectName: "cpuSummary"
                width: parent.width
                title: qsTr("CPU")
                value: Monitor.formatPercent(Monitor.metrics.cpuPercent)
                subtitle: qsTr("%1 logical processors").arg(Monitor.metrics.coreCount)
                values: Monitor.history.cpuPercent || []
                chartColor: FishUI.Theme.blueColor
                selected: rootWindow.currentPage === 1
                onClicked: rootWindow.currentPage = 1
            }

            ResourceSummary {
                objectName: "memorySummary"
                width: parent.width
                title: qsTr("Memory")
                value: Monitor.formatPercent(Monitor.metrics.memoryPercent)
                subtitle: Monitor.formatBytes(Monitor.metrics.memoryUsed) + " / " + Monitor.formatBytes(Monitor.metrics.memoryTotal)
                values: Monitor.history.memoryPercent || []
                chartColor: FishUI.Theme.purpleColor
                selected: rootWindow.currentPage === 2
                onClicked: rootWindow.currentPage = 2
            }

            ResourceSummary {
                objectName: "networkSummary"
                width: parent.width
                title: qsTr("Network")
                value: "↓ " + Monitor.formatRate(Monitor.metrics.download)
                subtitle: "↑ " + Monitor.formatRate(Monitor.metrics.upload)
                values: Monitor.history.download || []
                secondaryValues: Monitor.history.upload || []
                chartColor: FishUI.Theme.greenColor
                secondaryColor: FishUI.Theme.blueColor
                maximum: 0
                selected: rootWindow.currentPage === 3
                onClicked: rootWindow.currentPage = 3
            }

            ResourceSummary {
                objectName: "diskSummary"
                width: parent.width
                title: qsTr("Disk")
                value: Monitor.formatRate(Monitor.metrics.diskRead)
                subtitle: qsTr("Write %1").arg(Monitor.formatRate(Monitor.metrics.diskWrite))
                values: Monitor.history.diskRead || []
                secondaryValues: Monitor.history.diskWrite || []
                chartColor: FishUI.Theme.orangeColor
                secondaryColor: FishUI.Theme.purpleColor
                maximum: 0
                selected: rootWindow.currentPage === 4
                onClicked: rootWindow.currentPage = 4
            }
        }
    }

    Item {
        id: footer
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        width: rootWindow.sidebarWidth
        height: 52

        FishUI.ComboBox {
            id: intervalSelector
            objectName: "refreshInterval"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 12
            height: 28
            font.pointSize: 9
            model: [qsTr("Every second"), qsTr("Every 2 seconds"), qsTr("Every 5 seconds")]
            onActivated: Monitor.interval = [1000, 2000, 5000][currentIndex]
            Accessible.name: qsTr("Update interval")

            background: FishUI.RoundedRectangle {
                radius: FishUI.Theme.smallRadius
                color: FishUI.Theme.secondBackgroundColor
                border.width: 1
                border.color: intervalSelector.activeFocus || intervalSelector.menuVisible
                              ? FishUI.Theme.highlightColor : color
            }
        }
    }

    Item {
        anchors.left: parent.left
        anchors.leftMargin: rootWindow.sidebarWidth
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: rootWindow.header.height
        anchors.bottom: parent.bottom
        clip: true

        ProcessesPage {
            id: processesPage
            anchors.fill: parent
            visible: rootWindow.currentPage === 0
            window: rootWindow
        }

        Loader {
            anchors.fill: parent
            active: rootWindow.currentPage > 0
            sourceComponent: ResourcePage {
                page: rootWindow.currentPage
            }
        }
    }

    FishUI.Label {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        text: Monitor.error
        visible: text.length > 0
        color: FishUI.Theme.redColor
    }

    FishUI.ConfirmDialog {
        id: endDialog
        objectName: "endProcessDialog"
        parentWindow: rootWindow
        blurEnabled: false
        showOnCompleted: false
        property bool force: false
        property int targetPid: 0
        property string targetStart: ""
        property string targetName: ""
        title: force ? qsTr("Force stop %1?").arg(targetName) : qsTr("End %1?").arg(targetName)
        text: force ? qsTr("The process will stop immediately. Unsaved work may be lost.") : qsTr("The process will be asked to close. Unsaved work may be lost.")
        confirmText: force ? qsTr("Force Stop") : qsTr("End Process")
        destructive: true
        onAccepted: {
            var error = Monitor.endProcess(targetPid, targetStart, force);
            if (error.length > 0)
                rootWindow.showPassiveNotification(error);
        }
    }

    ProcessDetails {
        id: processDetails
        parentWindow: rootWindow
        pid: rootWindow.selectedPid
        startTicks: rootWindow.selectedStart
    }

    Shortcut {
        sequences: [StandardKey.Find]
        onActivated: {
            rootWindow.currentPage = 0;
            processesPage.focusSearch();
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: rootWindow.currentPage !== 0 || processesPage.editingSearch
        onActivated: {
            rootWindow.currentPage = 0;
            processesPage.clearSearch();
        }
    }

    function confirmEnd(force) {
        if (!selectedProcess.canManage)
            return;
        endDialog.force = force;
        endDialog.targetPid = selectedPid;
        endDialog.targetStart = selectedStart;
        endDialog.targetName = selectedProcess.name;
        endDialog.open();
    }

    function openDetails() {
        if (selectedProcess.pid)
            processDetails.open();
    }
}
