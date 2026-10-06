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
    height: 612
    minimumWidth: 480
    maximumWidth: 480
    minimumHeight: 612
    maximumHeight: 612
    blurEnabled: false
    background.color: FishUI.Theme.backgroundColor
    background.opacity: 1

    headerItem: Item {
        FishUI.Label {
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            text: dialog.title
            font.pointSize: 10
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
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

        ListCard {
            InfoRow {
                text: qsTr("User")
                value: dialog.process.user || "—"
            }
            InfoRow {
                text: qsTr("State")
                value: dialog.process.state || qsTr("Exited")
                visible: !dialog.application
            }
            InfoRow {
                text: qsTr("Processes")
                value: dialog.process.processCount || "—"
                visible: dialog.application
            }
            InfoRow {
                text: qsTr("CPU")
                value: Monitor.formatPercent(dialog.process.cpu || 0)
            }
            InfoRow {
                text: qsTr("Memory")
                value: Monitor.formatBytes(dialog.process.memory || 0)
            }
            InfoRow {
                text: qsTr("Threads")
                value: dialog.process.threads || "—"
            }
            InfoRow {
                text: qsTr("Nice value")
                value: dialog.process.nice === undefined ? "—" : dialog.process.nice
                visible: !dialog.application
            }
            InfoRow {
                text: qsTr("Parent process")
                value: dialog.process.parentPid || "—"
                visible: !dialog.application
            }
        }

        SectionLabel {
            text: dialog.application ? qsTr("Processes") : qsTr("Command")
        }

        FishUI.RoundedRectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: dialog.application
            radius: FishUI.Theme.bigRadius
            color: FishUI.Theme.secondBackgroundColor

            ListView {
                id: memberList
                objectName: "applicationProcesses"
                anchors.fill: parent
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: dialog.application && dialog.process.members ? dialog.process.members.length : 0
                FishUI.WheelHandler {
                    target: memberList
                }
                FishUI.ScrollBar.vertical: FishUI.ScrollBar {}

                delegate: Item {
                    id: memberRow
                    required property int index
                    readonly property var member: dialog.process.members[index] || ({})
                    width: memberList.width
                    height: 44

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        spacing: 12
                        FishUI.Label {
                            Layout.fillWidth: true
                            text: memberRow.member.name || ""
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                        }
                        FishUI.Label {
                            text: Monitor.formatBytes(memberRow.member.memory || 0)
                            color: FishUI.Theme.disabledTextColor
                        }
                        FishUI.Label {
                            Layout.preferredWidth: 64
                            text: memberRow.member.pid || ""
                            color: FishUI.Theme.disabledTextColor
                            horizontalAlignment: Text.AlignRight
                        }
                    }

                    Separator {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        visible: memberRow.index < memberList.count - 1
                    }
                }
            }
        }

        FishUI.RoundedRectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !dialog.application
            radius: FishUI.Theme.bigRadius
            color: FishUI.Theme.secondBackgroundColor

            ScrollView {
                id: commandView
                anchors.fill: parent
                clip: true
                FishUI.ScrollBar.vertical: FishUI.ScrollBar {}

                FishUI.TextArea {
                    width: commandView.availableWidth
                    // The rows' text inset above.
                    leftPadding: 16
                    rightPadding: 16
                    topPadding: 12
                    bottomPadding: 12
                    // No field underline: the card is the frame.
                    background: null
                    text: dialog.process.command || dialog.process.executable || "—"
                    textFormat: TextEdit.PlainText
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.WrapAnywhere
                }
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
