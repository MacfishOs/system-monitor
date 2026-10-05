// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FishUI as FishUI

Flickable {
    id: view
    objectName: "resourcePage"
    required property int page
    readonly property var metrics: Monitor.metrics
    contentHeight: content.height + 36
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    FishUI.WheelHandler {
        target: view
    }
    FishUI.ScrollBar.vertical: FishUI.ScrollBar {}

    ColumnLayout {
        id: content
        x: 24
        y: 12
        width: parent.width - 48
        spacing: 20

        ChartCard {
            objectName: "resourceChart"
            Layout.fillWidth: true
            Layout.preferredHeight: 276
            title: ["", qsTr("Processor activity"), qsTr("Memory usage"), qsTr("Network activity"), qsTr("Disk activity")][view.page]
            value: view.page === 1 ? Monitor.formatPercent(view.metrics.cpuPercent)
                   : view.page === 2 ? Monitor.formatPercent(view.metrics.memoryPercent)
                   : view.page === 3 ? Monitor.formatRate(view.metrics.download)
                                     : Monitor.formatRate(view.metrics.diskRead)
            subtitle: view.page === 1 ? view.metrics.cpuName
                      : view.page === 2 ? qsTr("%1 of %2 in use").arg(Monitor.formatBytes(view.metrics.memoryUsed)).arg(Monitor.formatBytes(view.metrics.memoryTotal))
                      : view.page === 3 ? qsTr("Physical network interfaces") : qsTr("Physical disks")
            chartColor: [FishUI.Theme.blueColor, FishUI.Theme.blueColor, FishUI.Theme.purpleColor,
                         FishUI.Theme.greenColor, FishUI.Theme.orangeColor][view.page]
            secondaryColor: view.page === 3 ? FishUI.Theme.blueColor : FishUI.Theme.purpleColor
            values: view.page === 1 ? Monitor.history.cpuPercent || []
                    : view.page === 2 ? Monitor.history.memoryPercent || []
                    : view.page === 3 ? Monitor.history.download || [] : Monitor.history.diskRead || []
            secondaryValues: view.page === 3 ? Monitor.history.upload || []
                             : view.page === 4 ? Monitor.history.diskWrite || [] : []
            maximum: view.page <= 2 ? 100 : 0
            primaryLegend: view.page === 3 ? qsTr("Download") : view.page === 4 ? qsTr("Read") : ""
            secondaryLegend: view.page === 3 ? qsTr("Upload") : view.page === 4 ? qsTr("Write") : ""
        }

        FishUI.Label {
            text: qsTr("Logical processors")
            font.weight: Font.Medium
            font.pointSize: 9
            visible: view.page === 1
        }

        GridLayout {
            Layout.fillWidth: true
            columns: Math.max(2, Math.floor((content.width + 12) / 162))
            rowSpacing: 12
            columnSpacing: 12
            visible: view.page === 1

            Repeater {
                model: view.page === 1 ? view.metrics.coreCount : 0
                delegate: FishUI.RoundedRectangle {
                    required property int index
                    readonly property real usage: view.metrics.cores[index] || 0
                    Layout.fillWidth: true
                    Layout.preferredHeight: 66
                    radius: FishUI.Theme.smallRadius
                    color: FishUI.Theme.secondBackgroundColor
                    FishUI.Label {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.margins: 12
                        text: qsTr("CPU %1").arg(index + 1)
                        font.pointSize: 8
                    }
                    FishUI.Label {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 12
                        text: Monitor.formatPercent(usage)
                        font.pointSize: 8
                    }
                    FishUI.ProgressBar {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: 12
                        from: 0
                        to: 100
                        value: usage
                    }
                }
            }
        }

        FishUI.RoundedRectangle {
            Layout.fillWidth: true
            implicitHeight: memoryDetails.implicitHeight + 32
            radius: FishUI.Theme.bigRadius
            color: FishUI.Theme.secondBackgroundColor
            visible: view.page === 2

            ColumnLayout {
                id: memoryDetails
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 16
                spacing: 4
                DetailRow {
                    Layout.fillWidth: true
                    label: qsTr("In use")
                    value: Monitor.formatBytes(view.metrics.memoryUsed)
                }
                DetailRow {
                    Layout.fillWidth: true
                    label: qsTr("Available")
                    value: Monitor.formatBytes(view.metrics.memoryAvailable)
                }
                DetailRow {
                    Layout.fillWidth: true
                    label: qsTr("Cached")
                    value: Monitor.formatBytes(view.metrics.memoryCache)
                }
                DetailRow {
                    Layout.fillWidth: true
                    label: qsTr("Total memory")
                    value: Monitor.formatBytes(view.metrics.memoryTotal)
                }
                DetailRow {
                    Layout.fillWidth: true
                    label: qsTr("Swap")
                    value: view.metrics.swapTotal > 0 ? Monitor.formatBytes(view.metrics.swapUsed) + " / " + Monitor.formatBytes(view.metrics.swapTotal) : qsTr("Not configured")
                }
            }
        }

        FishUI.Label {
            Layout.fillWidth: true
            text: qsTr("Available memory includes caches the system can reclaim when applications need it.")
            font.pointSize: 9
            wrapMode: Text.Wrap
            color: FishUI.Theme.disabledTextColor
            visible: view.page === 2
        }

        FishUI.RoundedRectangle {
            Layout.fillWidth: true
            implicitHeight: networkDetails.implicitHeight + 32
            radius: FishUI.Theme.bigRadius
            color: FishUI.Theme.secondBackgroundColor
            visible: view.page === 3

            ColumnLayout {
                id: networkDetails
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 16
                spacing: 4
                DetailRow {
                    Layout.fillWidth: true
                    label: qsTr("Download")
                    value: Monitor.formatRate(view.metrics.download)
                }
                DetailRow {
                    Layout.fillWidth: true
                    label: qsTr("Upload")
                    value: Monitor.formatRate(view.metrics.upload)
                }
                DetailRow {
                    Layout.fillWidth: true
                    label: qsTr("Total received")
                    value: Monitor.formatBytes(view.metrics.received)
                }
                DetailRow {
                    Layout.fillWidth: true
                    label: qsTr("Total sent")
                    value: Monitor.formatBytes(view.metrics.sent)
                }
            }
        }

        FishUI.Label {
            text: qsTr("Network interfaces")
            font.weight: Font.Medium
            font.pointSize: 9
            visible: view.page === 3
        }

        Repeater {
            model: view.page === 3 ? view.metrics.interfaces.length : 0
            delegate: FishUI.RoundedRectangle {
                required property int index
                readonly property var adapter: view.metrics.interfaces[index] || ({})
                Layout.fillWidth: true
                implicitHeight: interfaceDetails.implicitHeight + 32
                radius: FishUI.Theme.bigRadius
                color: FishUI.Theme.secondBackgroundColor
                ColumnLayout {
                    id: interfaceDetails
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 16
                    spacing: 4
                    FishUI.Label {
                        text: adapter.name || ""
                        textFormat: Text.PlainText
                        font.weight: Font.Medium
                        font.pointSize: 10
                    }
                    DetailRow {
                        Layout.fillWidth: true
                        label: qsTr("State")
                        value: adapter.state === "up" ? qsTr("Connected") : adapter.state === "down" ? qsTr("Disconnected") : qsTr("Unknown")
                    }
                    DetailRow {
                        Layout.fillWidth: true
                        label: qsTr("Download")
                        value: Monitor.formatRate(adapter.download)
                    }
                    DetailRow {
                        Layout.fillWidth: true
                        label: qsTr("Upload")
                        value: Monitor.formatRate(adapter.upload)
                    }
                    DetailRow {
                        Layout.fillWidth: true
                        label: qsTr("Hardware address")
                        value: adapter.address || "—"
                    }
                }
            }
        }

        FishUI.Label {
            Layout.fillWidth: true
            text: qsTr("The overview counts physical interfaces once. Virtual interfaces are listed separately.")
            font.pointSize: 9
            wrapMode: Text.Wrap
            color: FishUI.Theme.disabledTextColor
            visible: view.page === 3
        }

        Repeater {
            model: view.page === 4 ? view.metrics.disks.length : 0
            delegate: FishUI.RoundedRectangle {
                required property int index
                readonly property var disk: view.metrics.disks[index] || ({})
                Layout.fillWidth: true
                implicitHeight: diskDetails.implicitHeight + 32
                radius: FishUI.Theme.bigRadius
                color: FishUI.Theme.secondBackgroundColor
                ColumnLayout {
                    id: diskDetails
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 16
                    spacing: 4
                    FishUI.Label {
                        Layout.fillWidth: true
                        text: disk.model || disk.name || ""
                        textFormat: Text.PlainText
                        font.weight: Font.Medium
                        font.pointSize: 10
                        elide: Text.ElideRight
                    }
                    DetailRow {
                        Layout.fillWidth: true
                        label: qsTr("Device")
                        value: "/dev/" + (disk.name || "")
                    }
                    DetailRow {
                        Layout.fillWidth: true
                        label: qsTr("Capacity")
                        value: Monitor.formatBytes(disk.size)
                    }
                    DetailRow {
                        Layout.fillWidth: true
                        label: qsTr("Read")
                        value: Monitor.formatRate(disk.read)
                    }
                    DetailRow {
                        Layout.fillWidth: true
                        label: qsTr("Write")
                        value: Monitor.formatRate(disk.write)
                    }
                }
            }
        }

        FishUI.Label {
            text: qsTr("Storage")
            font.weight: Font.Medium
            font.pointSize: 9
            visible: view.page === 4
        }

        Repeater {
            model: view.page === 4 ? view.metrics.volumes.length : 0
            delegate: FishUI.RoundedRectangle {
                required property int index
                readonly property var volume: view.metrics.volumes[index] || ({})
                Layout.fillWidth: true
                implicitHeight: volumeDetails.implicitHeight + 32
                radius: FishUI.Theme.bigRadius
                color: FishUI.Theme.secondBackgroundColor
                ColumnLayout {
                    id: volumeDetails
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 16
                    spacing: 12
                    RowLayout {
                        Layout.fillWidth: true
                        FishUI.Label {
                            Layout.fillWidth: true
                            text: volume.mount || ""
                            textFormat: Text.PlainText
                            font.pointSize: 10
                            font.weight: Font.Medium
                            elide: Text.ElideMiddle
                        }
                        FishUI.Label {
                            text: qsTr("%1 available").arg(Monitor.formatBytes(volume.available))
                            font.pointSize: 9
                            color: FishUI.Theme.disabledTextColor
                        }
                    }
                    FishUI.ProgressBar {
                        Layout.fillWidth: true
                        from: 0
                        to: volume.total || 0
                        value: volume.used || 0
                    }
                    DetailRow {
                        Layout.fillWidth: true
                        label: volume.type || ""
                        value: qsTr("%1 of %2 used").arg(Monitor.formatBytes(volume.used)).arg(Monitor.formatBytes(volume.total))
                    }
                }
            }
        }

        FishUI.Label {
            Layout.fillWidth: true
            text: qsTr("Process CPU usage is shown as a share of the whole processor.")
            font.pointSize: 9
            wrapMode: Text.Wrap
            color: FishUI.Theme.disabledTextColor
            visible: view.page === 1
        }
    }
}
