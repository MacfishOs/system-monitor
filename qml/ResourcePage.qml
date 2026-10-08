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
    readonly property bool gpuTemperatureChart: page === 5 && !metrics.gpuHasBusy
    topMargin: 8
    bottomMargin: 24
    leftMargin: 12
    rightMargin: 12
    contentWidth: width - (leftMargin + rightMargin)
    contentHeight: content.height
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    // Flickable does not include topMargin in its initial content position.
    Component.onCompleted: contentY = -topMargin

    FishUI.WheelHandler {
        target: view
    }
    FishUI.ScrollBar.vertical: FishUI.ScrollBar {}

    ColumnLayout {
        id: content
        width: view.contentWidth
        spacing: 20

        ChartCard {
            objectName: "resourceChart"
            Layout.fillWidth: true
            Layout.preferredHeight: 276
            title: view.gpuTemperatureChart ? qsTr("Graphics temperature")
                                            : ["", qsTr("Processor activity"), qsTr("Memory usage"), qsTr("Network activity"),
                                               qsTr("Disk activity"), qsTr("Graphics activity")][view.page]
            value: view.page === 1 ? Monitor.formatPercent(view.metrics.cpuPercent)
                   : view.page === 2 ? Monitor.formatPercent(view.metrics.memoryPercent)
                   : view.page === 3 ? Monitor.formatRate(view.metrics.download)
                   : view.page === 4 ? Monitor.formatRate(view.metrics.diskRead)
                   : view.gpuTemperatureChart ? Monitor.formatTemperature(view.metrics.gpuTemperature)
                                              : Monitor.formatPercent(view.metrics.gpuPercent)
            subtitle: view.page === 1 ? view.metrics.cpuName
                      : view.page === 2 ? qsTr("%1 of %2 in use").arg(Monitor.formatBytes(view.metrics.memoryUsed)).arg(Monitor.formatBytes(view.metrics.memoryTotal))
                      : view.page === 3 ? qsTr("Physical network interfaces")
                      : view.page === 4 ? qsTr("Physical disks") : view.metrics.gpuName
            chartColor: [FishUI.Theme.blueColor, FishUI.Theme.blueColor, FishUI.Theme.purpleColor,
                         FishUI.Theme.greenColor, FishUI.Theme.orangeColor, FishUI.Theme.pinkColor][view.page]
            secondaryColor: view.page === 3 ? FishUI.Theme.blueColor : FishUI.Theme.purpleColor
            values: view.page === 1 ? Monitor.history.cpuPercent || []
                    : view.page === 2 ? Monitor.history.memoryPercent || []
                    : view.page === 3 ? Monitor.history.download || []
                    : view.page === 4 ? Monitor.history.diskRead || []
                    : view.gpuTemperatureChart ? Monitor.history.gpuTemperature || [] : Monitor.history.gpuPercent || []
            secondaryValues: view.page === 3 ? Monitor.history.upload || []
                             : view.page === 4 ? Monitor.history.diskWrite || [] : []
            maximum: view.page <= 2 || view.page === 5 ? 100 : 0
            scaleLabel: view.gpuTemperatureChart ? Monitor.formatTemperature(100) : ""
            primaryLegend: view.page === 3 ? qsTr("Download") : view.page === 4 ? qsTr("Read") : ""
            secondaryLegend: view.page === 3 ? qsTr("Upload") : view.page === 4 ? qsTr("Write") : ""
        }

        FishUI.Label {
            leftPadding: 16
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

        ListCard {
            visible: view.page === 2

            InfoRow {
                text: qsTr("In use")
                value: Monitor.formatBytes(view.metrics.memoryUsed)
            }
            InfoRow {
                text: qsTr("Available")
                value: Monitor.formatBytes(view.metrics.memoryAvailable)
            }
            InfoRow {
                text: qsTr("Cached")
                value: Monitor.formatBytes(view.metrics.memoryCache)
            }
            InfoRow {
                text: qsTr("Total memory")
                value: Monitor.formatBytes(view.metrics.memoryTotal)
            }
            InfoRow {
                text: qsTr("Swap")
                value: view.metrics.swapTotal > 0 ? Monitor.formatBytes(view.metrics.swapUsed) + " / " + Monitor.formatBytes(view.metrics.swapTotal) : qsTr("Not configured")
            }
        }

        ListCard {
            visible: view.page === 3

            InfoRow {
                text: qsTr("Download")
                value: Monitor.formatRate(view.metrics.download)
            }
            InfoRow {
                text: qsTr("Upload")
                value: Monitor.formatRate(view.metrics.upload)
            }
            InfoRow {
                text: qsTr("Total received")
                value: Monitor.formatBytes(view.metrics.received)
            }
            InfoRow {
                text: qsTr("Total sent")
                value: Monitor.formatBytes(view.metrics.sent)
            }
        }

        FishUI.Label {
            leftPadding: 16
            text: qsTr("Network interfaces")
            font.weight: Font.Medium
            font.pointSize: 9
            visible: view.page === 3
        }

        Repeater {
            model: view.page === 3 ? view.metrics.interfaces.length : 0
            delegate: ColumnLayout {
                required property int index
                readonly property var adapter: view.metrics.interfaces[index] || ({})
                Layout.fillWidth: true
                spacing: 8

                SectionLabel {
                    text: adapter.name || ""
                }
                ListCard {
                    InfoRow {
                        text: qsTr("State")
                        value: adapter.state === "up" ? qsTr("Connected") : adapter.state === "down" ? qsTr("Disconnected") : qsTr("Unknown")
                    }
                    InfoRow {
                        text: qsTr("Download")
                        value: Monitor.formatRate(adapter.download)
                    }
                    InfoRow {
                        text: qsTr("Upload")
                        value: Monitor.formatRate(adapter.upload)
                    }
                    InfoRow {
                        text: qsTr("Hardware address")
                        value: adapter.address || "—"
                    }
                }
            }
        }

        Repeater {
            model: view.page === 4 ? view.metrics.disks.length : 0
            delegate: ColumnLayout {
                required property int index
                readonly property var disk: view.metrics.disks[index] || ({})
                Layout.fillWidth: true
                spacing: 8

                SectionLabel {
                    text: disk.model || disk.name || ""
                }
                ListCard {
                    InfoRow {
                        text: qsTr("Device")
                        value: "/dev/" + (disk.name || "")
                    }
                    InfoRow {
                        text: qsTr("Capacity")
                        value: Monitor.formatBytes(disk.size)
                    }
                    InfoRow {
                        text: qsTr("Read")
                        value: Monitor.formatRate(disk.read)
                    }
                    InfoRow {
                        text: qsTr("Write")
                        value: Monitor.formatRate(disk.write)
                    }
                }
            }
        }

        FishUI.Label {
            leftPadding: 16
            text: qsTr("Storage")
            font.weight: Font.Medium
            font.pointSize: 9
            visible: view.page === 4
        }

        Repeater {
            model: view.page === 4 ? view.metrics.volumes.length : 0
            delegate: ColumnLayout {
                required property int index
                readonly property var volume: view.metrics.volumes[index] || ({})
                Layout.fillWidth: true
                spacing: 8

                SectionLabel {
                    text: volume.mount || ""
                    elide: Text.ElideMiddle
                }
                ListCard {
                    Item {
                        Layout.fillWidth: true
                        implicitHeight: 16 + usage.implicitHeight

                        FishUI.ProgressBar {
                            id: usage
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.leftMargin: 16
                            anchors.rightMargin: 16
                            from: 0
                            to: volume.total || 0
                            value: volume.used || 0
                        }
                    }
                    InfoRow {
                        text: qsTr("Used")
                        value: qsTr("%1 of %2").arg(Monitor.formatBytes(volume.used)).arg(Monitor.formatBytes(volume.total))
                    }
                    InfoRow {
                        text: qsTr("Available")
                        value: Monitor.formatBytes(volume.available)
                    }
                    InfoRow {
                        text: qsTr("File system")
                        value: volume.type || "—"
                    }
                }
            }
        }

        Repeater {
            model: view.page === 5 ? view.metrics.gpus.length : 0
            delegate: ColumnLayout {
                required property int index
                readonly property var gpu: view.metrics.gpus[index] || ({})
                Layout.fillWidth: true
                spacing: 8

                SectionLabel {
                    text: gpu.name || ""
                }
                ListCard {
                    InfoRow {
                        text: qsTr("Driver")
                        value: gpu.driver || "—"
                    }
                    InfoRow {
                        text: qsTr("State")
                        value: gpu.active ? qsTr("Active") : qsTr("Powered off")
                    }
                    InfoRow {
                        text: qsTr("Activity")
                        value: Monitor.formatPercent(gpu.busy || 0)
                        visible: !!gpu.hasBusy
                    }
                    InfoRow {
                        text: qsTr("Temperature")
                        value: gpu.active && gpu.temperature >= 0 ? Monitor.formatTemperature(gpu.temperature) : "—"
                        visible: !!gpu.hasTemperature
                    }
                    InfoRow {
                        text: qsTr("Clock speed")
                        value: gpu.active ? qsTr("%1 MHz").arg(Math.round(gpu.frequency || 0)) : "—"
                        visible: !!gpu.hasFrequency
                    }
                    InfoRow {
                        text: qsTr("Video memory")
                        value: gpu.active ? Monitor.formatBytes(gpu.vramUsed) + " / " + Monitor.formatBytes(gpu.vramTotal) : "—"
                        visible: !!gpu.hasVram
                    }
                }
            }
        }
    }
}
