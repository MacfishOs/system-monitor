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
    property bool selectedApplication: true
    readonly property int sidebarWidth: 240
    // The title and window buttons end on this line; content spacing is measured from it.
    readonly property int headerBaseline: 28
    readonly property var pageTitles: [qsTr("Processes"), qsTr("CPU"), qsTr("Memory"), qsTr("Network"), qsTr("Disk"), qsTr("GPU")]
    readonly property var selectedProcess: {
        Monitor.metrics;
        return selectedApplication ? Monitor.application(selectedPid, selectedStart)
                                   : Monitor.process(selectedPid, selectedStart);
    }

    function sumSeries(first, second) {
        first = first || [];
        second = second || [];
        return first.map((value, index) => value + (second[index] || 0));
    }

    LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    headerItem: Item {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: rootWindow.windowButtonsSpacing
            spacing: 12

            FishUI.AlignedLabel {
                id: applicationTitle
                objectName: "applicationTitle"
                Layout.preferredWidth: rootWindow.sidebarWidth - 20
                textBaseline: rootWindow.headerBaseline
                text: rootWindow.title
                font.pointSize: 11
                color: rootWindow.active ? FishUI.Theme.textColor : FishUI.Theme.disabledTextColor
                elide: Text.ElideRight
            }

            FishUI.AlignedLabel {
                objectName: "pageTitle"
                Layout.fillWidth: true
                textBaseline: rootWindow.headerBaseline
                text: rootWindow.pageTitles[rootWindow.currentPage]
                font.pointSize: 11
                color: rootWindow.active ? FishUI.Theme.textColor : FishUI.Theme.disabledTextColor
            }

            FishUI.HeaderButton {
                id: intervalButton
                objectName: "refreshInterval"
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: rootWindow.windowButtonsTopMargin
                size: rootWindow.header.buttonSize
                artwork: "menu"
                checked: intervalMenu.visible
                onClicked: intervalMenu.popup()
                Accessible.name: qsTr("Update interval")
            }
        }
    }

    Flickable {
        id: sidebar
        objectName: "resourceSidebar"
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.topMargin: rootWindow.header.height
        anchors.bottom: parent.bottom
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
                hoverEnabled: false
                highlighted: rootWindow.currentPage === 0
                onClicked: rootWindow.currentPage = 0
                Accessible.name: qsTr("Processes")

                // Same plate as the ResourceSummary cards below.
                background: FishUI.RoundedRectangle {
                    radius: FishUI.Theme.mediumRadius
                    color: processesButton.highlighted ? Qt.alpha(FishUI.Theme.highlightColor, FishUI.Theme.darkMode ? 0.15 : 0.09)
                                                       : FishUI.Theme.secondBackgroundColor
                    border.width: processesButton.highlighted || processesButton.visualFocus ? 1 / FishUI.Dpi.ratio : 0
                    border.color: Qt.alpha(FishUI.Theme.highlightColor, 0.5)
                }

                // Icon and title columns line up with the ResourceSummary cards.
                padding: 0
                leftPadding: 12
                rightPadding: 12
                contentItem: RowLayout {
                    spacing: 8
                    FishUI.IconItem {
                        Layout.alignment: Qt.AlignTop
                        Layout.topMargin: processesLabel.capCenter - height / 2
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                        source: "qrc:/icons/processes.svg"
                        color: processesButton.highlighted ? FishUI.Theme.highlightColor : FishUI.Theme.textColor
                    }
                    // Capitals centred in the row, not the line box.
                    FishUI.AlignedLabel {
                        id: processesLabel
                        Layout.fillWidth: true
                        textBaseline: (processesButton.height + capHeight) / 2
                        text: qsTr("Processes")
                        font.weight: Font.Medium
                    }
                    FishUI.AlignedLabel {
                        textBaseline: processesLabel.textBaseline
                        text: Monitor.ready ? Monitor.metrics.processCount : ""
                        color: FishUI.Theme.disabledTextColor
                        font.pointSize: 9
                    }
                }
            }

            ResourceSummary {
                objectName: "cpuSummary"
                iconName: "cpu"
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
                iconName: "memory"
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
                iconName: "network"
                width: parent.width
                title: qsTr("Network")
                value: "↓ " + Monitor.formatRate(Monitor.metrics.download)
                subtitle: "↑ " + Monitor.formatRate(Monitor.metrics.upload)
                values: rootWindow.sumSeries(Monitor.history.download, Monitor.history.upload)
                chartColor: FishUI.Theme.greenColor
                maximum: 0
                scaleFloor: 1024 * 1024
                selected: rootWindow.currentPage === 3
                onClicked: rootWindow.currentPage = 3
            }

            ResourceSummary {
                objectName: "diskSummary"
                iconName: "disk"
                width: parent.width
                title: qsTr("Disk")
                value: Monitor.formatRate(Monitor.metrics.diskRead)
                subtitle: qsTr("Write %1").arg(Monitor.formatRate(Monitor.metrics.diskWrite))
                values: rootWindow.sumSeries(Monitor.history.diskRead, Monitor.history.diskWrite)
                chartColor: FishUI.Theme.orangeColor
                maximum: 0
                scaleFloor: 1024 * 1024
                selected: rootWindow.currentPage === 4
                onClicked: rootWindow.currentPage = 4
            }

            ResourceSummary {
                objectName: "gpuSummary"
                iconName: "gpu"
                width: parent.width
                visible: Monitor.metrics.gpuAvailable
                title: qsTr("GPU")
                value: Monitor.metrics.gpuHasBusy ? Monitor.formatPercent(Monitor.metrics.gpuPercent)
                                                  : Monitor.formatTemperature(Monitor.metrics.gpuTemperature)
                subtitle: Monitor.metrics.gpuHasBusy && Monitor.metrics.gpuHasTemperature
                          ? Monitor.formatTemperature(Monitor.metrics.gpuTemperature) : Monitor.metrics.gpuName
                values: (Monitor.metrics.gpuHasBusy ? Monitor.history.gpuPercent : Monitor.history.gpuTemperature) || []
                chartColor: FishUI.Theme.pinkColor
                selected: rootWindow.currentPage === 5
                onClicked: rootWindow.currentPage = 5
            }
        }
    }

    FishUI.DesktopMenu {
        id: intervalMenu
        pointerGrabEnabled: true

        Repeater {
            model: [
                { text: qsTr("Update Every Second"), interval: 1000 },
                { text: qsTr("Update Every 2 Seconds"), interval: 2000 },
                { text: qsTr("Update Every 5 Seconds"), interval: 5000 }
            ]

            FishUI.MenuItem {
                text: modelData.text
                checked: Monitor.interval === modelData.interval
                onTriggered: Monitor.interval = modelData.interval
            }
        }
    }

    ProcessesPage {
        id: processesPage
        anchors.left: parent.left
        anchors.leftMargin: rootWindow.sidebarWidth
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: rootWindow.headerBaseline
        anchors.bottom: parent.bottom
        visible: rootWindow.currentPage === 0
        window: rootWindow
    }

    Item {
        anchors.left: parent.left
        anchors.leftMargin: rootWindow.sidebarWidth
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: rootWindow.header.height
        anchors.bottom: parent.bottom
        clip: true

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
        property bool application: false
        property int targetPid: 0
        property string targetStart: ""
        property string targetName: ""
        title: force ? qsTr("Force quit %1?").arg(targetName) : qsTr("Quit %1?").arg(targetName)
        text: application ? (force ? qsTr("All of its processes will stop immediately. Unsaved work may be lost.")
                                   : qsTr("The application will be asked to quit. Unsaved work may be lost."))
                          : (force ? qsTr("The process will stop immediately. Unsaved work may be lost.")
                                   : qsTr("The process will be asked to quit. Unsaved work may be lost."))
        confirmText: force ? qsTr("Force Quit") : qsTr("Quit")
        destructive: true
        onAccepted: {
            var error = application ? Monitor.endApplication(targetPid, targetStart, force)
                                    : Monitor.endProcess(targetPid, targetStart, force);
            if (error.length > 0)
                rootWindow.showPassiveNotification(error);
        }
    }

    ProcessDetails {
        id: processDetails
        parentWindow: rootWindow
        pid: rootWindow.selectedPid
        startTicks: rootWindow.selectedStart
        application: rootWindow.selectedApplication
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
        endDialog.application = selectedApplication;
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
