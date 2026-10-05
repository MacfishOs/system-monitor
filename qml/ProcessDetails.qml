// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FishUI as FishUI

FishUI.Window {
    id: dialog
    objectName: "processDetailsWindow"
    property int pid: 0
    property string startTicks: ""
    property bool application: false
    readonly property var process: {
        Monitor.metrics;
        return application ? Monitor.application(pid, startTicks) : Monitor.process(pid, startTicks);
    }

    title: application ? qsTr("Application Details") : qsTr("Process Details")
    visible: false
    flags: Qt.Dialog | Qt.FramelessWindowHint
    modality: Qt.WindowModal
    minimizeButtonVisible: false
    width: 480
    height: 528
    minimumWidth: 480
    maximumWidth: 480
    minimumHeight: 528
    maximumHeight: 528
    blurEnabled: false
    background.color: FishUI.Theme.backgroundColor
    background.opacity: 1

    headerItem: Item {
        FishUI.Label {
            anchors.left: parent.left
            anchors.leftMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            text: dialog.title
            font.pointSize: 10
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        anchors.topMargin: 12
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            FishUI.IconItem {
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
                source: dialog.process.icon || "application-x-executable"
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                FishUI.Label {
                    Layout.fillWidth: true
                    text: dialog.process.name || (dialog.application ? qsTr("Application quit") : qsTr("Process exited"))
                    textFormat: Text.PlainText
                    font.pointSize: 12
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                }
                FishUI.Label {
                    text: qsTr("PID %1").arg(dialog.pid)
                    color: FishUI.Theme.disabledTextColor
                    font.pointSize: 9
                }
            }
        }

        FishUI.RoundedRectangle {
            Layout.fillWidth: true
            implicitHeight: details.implicitHeight + 24
            color: FishUI.Theme.secondBackgroundColor
            radius: FishUI.Theme.bigRadius
            ColumnLayout {
                id: details
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 12
                spacing: 0
                DetailRow {
                    Layout.fillWidth: true
                    label: qsTr("User")
                    value: dialog.process.user || "—"
                }
                DetailRow {
                    Layout.fillWidth: true
                    label: qsTr("State")
                    value: dialog.process.state || qsTr("Exited")
                    visible: !dialog.application
                }
                DetailRow {
                    Layout.fillWidth: true
                    label: qsTr("Processes")
                    value: dialog.process.processCount || "—"
                    visible: dialog.application
                }
                DetailRow {
                    Layout.fillWidth: true
                    label: qsTr("CPU")
                    value: Monitor.formatPercent(dialog.process.cpu || 0)
                }
                DetailRow {
                    Layout.fillWidth: true
                    label: qsTr("Memory")
                    value: Monitor.formatBytes(dialog.process.memory || 0)
                }
                DetailRow {
                    Layout.fillWidth: true
                    label: qsTr("Threads")
                    value: dialog.process.threads || "—"
                }
                DetailRow {
                    Layout.fillWidth: true
                    label: qsTr("Nice value")
                    value: dialog.process.nice === undefined ? "—" : dialog.process.nice
                    visible: !dialog.application
                }
                DetailRow {
                    Layout.fillWidth: true
                    label: qsTr("Parent process")
                    value: dialog.process.parentPid || "—"
                    visible: !dialog.application
                }
            }
        }

        FishUI.Label {
            text: dialog.application ? qsTr("Processes") : qsTr("Command")
            font.pointSize: 9
            color: FishUI.Theme.disabledTextColor
        }

        ListView {
            id: memberList
            objectName: "applicationProcesses"
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: dialog.application
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: dialog.application && dialog.process.members ? dialog.process.members.length : 0
            FishUI.WheelHandler {
                target: memberList
            }
            FishUI.ScrollBar.vertical: FishUI.ScrollBar {}

            delegate: RowLayout {
                required property int index
                readonly property var member: dialog.process.members[index] || ({})
                width: memberList.width
                height: 28
                spacing: 12
                FishUI.Label {
                    Layout.fillWidth: true
                    text: member.name || ""
                    textFormat: Text.PlainText
                    font.pointSize: 9
                    elide: Text.ElideRight
                }
                FishUI.Label {
                    text: Monitor.formatBytes(member.memory || 0)
                    font.pointSize: 9
                    color: FishUI.Theme.disabledTextColor
                }
                FishUI.Label {
                    Layout.preferredWidth: 64
                    text: member.pid || ""
                    font.pointSize: 9
                    color: FishUI.Theme.disabledTextColor
                    horizontalAlignment: Text.AlignRight
                }
            }
        }

        ScrollView {
            id: commandView
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !dialog.application
            clip: true
            FishUI.ScrollBar.vertical: FishUI.ScrollBar {}

            FishUI.TextArea {
                width: commandView.availableWidth
                text: dialog.process.command || dialog.process.executable || "—"
                textFormat: TextEdit.PlainText
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.WrapAnywhere
                font.pointSize: 9
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            FishUI.ConfirmDialogButton {
                text: qsTr("Copy")
                visible: !dialog.application
                enabled: !!dialog.process.pid
                onClicked: Monitor.copyText(dialog.process.command || dialog.process.executable || "")
            }
            FishUI.ConfirmDialogButton {
                text: qsTr("Done")
                primary: true
                onClicked: dialog.close()
            }
        }
    }

    Shortcut {
        sequence: "Escape"
        onActivated: dialog.close()
    }

    function open() {
        show();
        raise();
        requestActivate();
    }
}
