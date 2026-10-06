// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FishUI as FishUI

Item {
    id: page
    required property var window
    readonly property bool editingSearch: search.activeFocus
    readonly property bool applicationsMode: scope.currentIndex === 0
    readonly property var model: applicationsMode ? Monitor.applications : Monitor.processes
    readonly property int cpuWidth: 72
    readonly property int memoryWidth: 88
    readonly property int ioWidth: 88
    readonly property int pidWidth: 64

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        anchors.leftMargin: 12
        anchors.topMargin: 26
        anchors.bottomMargin: 12
        anchors.rightMargin: 24
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            FishUI.SegmentedControl {
                id: scope
                objectName: "processScope"
                Layout.preferredWidth: 357
                Layout.preferredHeight: 36
                currentIndex: Monitor.scope
                onCurrentIndexChanged: {
                    Monitor.scope = currentIndex
                    page.clearSelection()
                }
                FishUI.TabButton {
                    text: qsTr("Applications")
                    font.pointSize: 9
                }
                FishUI.TabButton {
                    text: qsTr("My Processes")
                    font.pointSize: 9
                }
                FishUI.TabButton {
                    text: qsTr("All Processes")
                    font.pointSize: 9
                }
            }

            FishUI.TextField {
                id: search
                objectName: "processSearch"
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                leftPadding: 34
                rightPadding: text.length > 0 ? 34 : 12
                topPadding: 0
                bottomPadding: 0
                placeholderText: qsTr("Search processes")
                font.pointSize: 9
                onTextChanged: {
                    Monitor.processes.search = text
                    Monitor.applications.search = text
                    page.clearSelection()
                }
                Accessible.name: qsTr("Search processes")

                background: FishUI.RoundedRectangle {
                    objectName: "processSearchBackground"
                    continuousCorners: false
                    radius: height / 2
                    color: FishUI.Theme.secondBackgroundColor
                    border.width: 1
                    border.color: search.activeFocus ? FishUI.Theme.highlightColor : FishUI.Theme.secondBackgroundColor

                    FishUI.RoundedRectangle {
                        anchors.fill: parent
                        anchors.margins: -2
                        continuousCorners: false
                        radius: parent.height / 2
                        inset: -2
                        color: "transparent"
                        border.width: 2
                        border.color: FishUI.Theme.highlightColor
                        opacity: 0.25
                        visible: search.activeFocus
                    }
                }

                FishUI.Icon {
                    objectName: "processSearchIcon"
                    x: 12
                    anchors.verticalCenter: parent.verticalCenter
                    width: 14
                    height: 14
                    name: "search"
                    color: FishUI.Theme.disabledTextColor
                }

                FishUI.Icon {
                    objectName: "processSearchClearIcon"
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    width: 14
                    height: 14
                    visible: search.text.length > 0
                    name: "clear"
                    color: FishUI.Theme.disabledTextColor
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Clear search")
                    Accessible.onPressAction: search.clear()

                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -7
                        onClicked: search.clear()
                    }
                }
            }
        }

        FishUI.RoundedRectangle {
            id: table
            objectName: "processTable"
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: FishUI.Theme.bigRadius
            color: FishUI.Theme.secondBackgroundColor

            ColumnLayout {
                anchors.fill: parent
                spacing: 0
                layer.enabled: true
                layer.effect: FishUI.OpacityMask {
                    maskSource: Item {
                        width: table.width
                        height: table.height

                        FishUI.RoundedRectangle {
                            anchors.fill: parent
                            radius: table.radius
                        }
                    }
                }

                Row {
                    id: columns
                    Layout.fillWidth: true
                    Layout.preferredHeight: 30
                    ColumnHeader {
                        width: columns.width - page.cpuWidth - page.memoryWidth - page.ioWidth * 2 - page.pidWidth
                        text: qsTr("Name")
                        column: "name"
                        numeric: false
                    }
                    ColumnHeader {
                        width: page.cpuWidth
                        text: qsTr("CPU")
                        column: "cpu"
                    }
                    ColumnHeader {
                        width: page.memoryWidth
                        text: qsTr("Memory")
                        column: "memory"
                    }
                    ColumnHeader {
                        width: page.ioWidth
                        text: qsTr("Read")
                        column: "read"
                    }
                    ColumnHeader {
                        width: page.ioWidth
                        text: qsTr("Write")
                        column: "write"
                    }
                    ColumnHeader {
                        width: page.pidWidth
                        text: qsTr("PID")
                        column: "pid"
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1 / FishUI.Dpi.ratio
                    color: Qt.alpha(FishUI.Theme.textColor, 0.07)
                }

                ListView {
                    id: list
                    objectName: "processList"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: page.model
                    boundsBehavior: Flickable.StopAtBounds
                    currentIndex: -1
                    keyNavigationEnabled: true
                    highlightMoveDuration: 0
                    highlightFollowsCurrentItem: false

                    FishUI.WheelHandler {
                        target: list
                    }
                    FishUI.ScrollBar.vertical: FishUI.ScrollBar {}

                    delegate: ItemDelegate {
                        id: row
                        required property int index
                        required property int pid
                        required property string displayName
                        required property string processName
                        required property string iconName
                        required property string userName
                        required property real cpu
                        required property real memory
                        required property real readRate
                        required property real writeRate
                        required property bool ioAvailable
                        required property string startTicks
                        width: list.width
                        height: 40
                        padding: 0
                        hoverEnabled: false
                        highlighted: page.window.selectedPid === pid && page.window.selectedStart === startTicks
                                     && page.window.selectedApplication === page.applicationsMode
                        onClicked: {
                            list.currentIndex = index;
                            page.select(index);
                            list.forceActiveFocus();
                        }
                        onDoubleClicked: page.window.openDetails()
                        Accessible.name: displayName + ", " + qsTr("CPU %1, memory %2, PID %3").arg(Monitor.formatPercent(cpu)).arg(Monitor.formatBytes(memory)).arg(pid)

                        background: Rectangle {
                            color: row.highlighted ? FishUI.Theme.highlightColor : row.index % 2 ? Qt.alpha(FishUI.Theme.textColor, 0.018) : "transparent"
                        }

                        contentItem: Row {
                            Item {
                                width: row.width - page.cpuWidth - page.memoryWidth - page.ioWidth * 2 - page.pidWidth
                                height: row.height
                                FishUI.IconItem {
                                    x: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 22
                                    height: 22
                                    source: row.iconName
                                }
                                FishUI.Label {
                                    x: 44
                                    width: parent.width - x - 12
                                    height: parent.height
                                    text: row.displayName
                                    textFormat: Text.PlainText
                                    font.pointSize: 9
                                    color: row.highlighted ? FishUI.Theme.highlightedTextColor : FishUI.Theme.textColor
                                    elide: Text.ElideRight
                                }
                            }

                            FishUI.Label {
                                width: page.cpuWidth
                                height: row.height
                                rightPadding: 12
                                text: Monitor.formatPercent(row.cpu)
                                color: row.highlighted ? FishUI.Theme.highlightedTextColor : FishUI.Theme.textColor
                                horizontalAlignment: Text.AlignRight
                                font.pointSize: 9
                            }
                            FishUI.Label {
                                width: page.memoryWidth
                                height: row.height
                                rightPadding: 12
                                text: Monitor.formatBytes(row.memory)
                                color: row.highlighted ? FishUI.Theme.highlightedTextColor : FishUI.Theme.textColor
                                horizontalAlignment: Text.AlignRight
                                font.pointSize: 9
                            }
                            FishUI.Label {
                                width: page.ioWidth
                                height: row.height
                                rightPadding: 12
                                text: row.ioAvailable ? Monitor.formatRate(row.readRate) : "—"
                                horizontalAlignment: Text.AlignRight
                                font.pointSize: 9
                                color: row.highlighted ? FishUI.Theme.highlightedTextColor : FishUI.Theme.disabledTextColor
                            }
                            FishUI.Label {
                                width: page.ioWidth
                                height: row.height
                                rightPadding: 12
                                text: row.ioAvailable ? Monitor.formatRate(row.writeRate) : "—"
                                horizontalAlignment: Text.AlignRight
                                font.pointSize: 9
                                color: row.highlighted ? FishUI.Theme.highlightedTextColor : FishUI.Theme.disabledTextColor
                            }
                            FishUI.Label {
                                width: page.pidWidth
                                height: row.height
                                rightPadding: 12
                                text: row.pid
                                horizontalAlignment: Text.AlignRight
                                font.pointSize: 9
                                color: row.highlighted ? FishUI.Theme.highlightedTextColor : FishUI.Theme.disabledTextColor
                            }
                        }

                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            onTapped: {
                                page.select(row.index);
                                processMenu.popup();
                            }
                        }
                    }

                    Keys.onUpPressed: {
                        currentIndex = Math.max(0, page.model.indexOf(page.window.selectedPid) - 1);
                        page.select(currentIndex);
                        positionViewAtIndex(currentIndex, ListView.Contain);
                    }
                    Keys.onDownPressed: {
                        currentIndex = Math.min(count - 1, page.model.indexOf(page.window.selectedPid) + 1);
                        page.select(currentIndex);
                        positionViewAtIndex(currentIndex, ListView.Contain);
                    }
                    Keys.onReturnPressed: page.window.openDetails()
                    Keys.onDeletePressed: page.window.confirmEnd(false)
                }
            }

            Column {
                anchors.centerIn: parent
                width: parent.width - 64
                spacing: 8
                visible: page.model.count === 0
                FishUI.Label {
                    width: parent.width
                    text: !Monitor.ready ? qsTr("Collecting system information…")
                                         : page.applicationsMode ? qsTr("No applications found") : qsTr("No processes found")
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter
                }
                FishUI.Label {
                    width: parent.width
                    text: Monitor.ready ? qsTr("Try a different name, user, or process ID.") : ""
                    color: FishUI.Theme.disabledTextColor
                    font.pointSize: 9
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        FishUI.ConfirmDialogButton {
            objectName: "endProcessButton"
            Layout.alignment: Qt.AlignRight
            text: page.applicationsMode ? qsTr("Quit Application") : qsTr("End Process")
            enabled: !!page.window.selectedProcess.canManage
            onClicked: page.window.confirmEnd(false)
        }
    }

    FishUI.DesktopMenu {
        id: processMenu
        objectName: "processContextMenu"
        parentItem: list
        FishUI.MenuItem {
            text: qsTr("Details")
            enabled: !!page.window.selectedProcess.pid
            onTriggered: page.window.openDetails()
        }
        FishUI.MenuItem {
            text: qsTr("Copy Process ID")
            onTriggered: Monitor.copyText(String(page.window.selectedPid))
        }
        FishUI.MenuSeparator {}
        FishUI.MenuItem {
            text: page.applicationsMode ? qsTr("Quit") : qsTr("End Process")
            enabled: !!page.window.selectedProcess.canManage
            onTriggered: page.window.confirmEnd(false)
        }
        FishUI.MenuItem {
            text: page.applicationsMode ? qsTr("Force Quit") : qsTr("Force Stop")
            enabled: !!page.window.selectedProcess.canManage
            onTriggered: page.window.confirmEnd(true)
        }
    }

    function select(index) {
        var process = page.model.get(index);
        page.window.selectedApplication = page.applicationsMode;
        page.window.selectedPid = process.pid || 0;
        page.window.selectedStart = process.startTicks || "";
    }

    function clearSelection() {
        page.window.selectedPid = 0
        page.window.selectedStart = ""
        page.window.selectedApplication = page.applicationsMode
    }

    function focusSearch() {
        search.forceActiveFocus();
        search.selectAll();
    }

    function clearSearch() {
        search.clear();
        list.forceActiveFocus();
    }
}
