// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import FishUI as FishUI
import Cutefish.SystemMonitor

FishUI.RoundedRectangle {
    id: control
    property string title
    property string value
    property string subtitle
    property var values: []
    property var secondaryValues: []
    property color chartColor: FishUI.Theme.blueColor
    property color secondaryColor: FishUI.Theme.purpleColor
    property real maximum: 100
    property string primaryLegend
    property string secondaryLegend
    property string scaleLabel

    implicitHeight: 276
    radius: FishUI.Theme.bigRadius
    color: FishUI.Theme.secondBackgroundColor

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 16
            FishUI.Label {
                Layout.fillWidth: true
                text: control.title
                font.pointSize: 11
                font.weight: Font.Medium
            }
            FishUI.Label {
                text: control.value
                font.pointSize: 18
                font.weight: Font.Medium
            }
        }

        FishUI.Label {
            Layout.fillWidth: true
            text: control.subtitle
            color: FishUI.Theme.disabledTextColor
            font.pointSize: 9
            elide: Text.ElideRight
        }

        FishUI.Label {
            objectName: "chartScaleLabel"
            Layout.alignment: Qt.AlignRight
            text: control.scaleLabel.length > 0 ? control.scaleLabel
                  : control.maximum > 0 ? "100%" : Monitor.formatRate(graph.effectiveMaximum)
            font.pointSize: 8
            color: FishUI.Theme.disabledTextColor
        }

        HistoryGraph {
            id: graph
            objectName: "historyGraph"
            Layout.fillWidth: true
            Layout.fillHeight: true
            values: control.values
            secondaryValues: control.secondaryValues
            timestamps: Monitor.history.timestamps || []
            duration: Monitor.historyDuration * 1000
            color: control.chartColor
            secondaryColor: control.secondaryColor
            gridColor: Qt.alpha(FishUI.Theme.textColor, 0.06)
            maximum: control.maximum
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            FishUI.Label {
                text: qsTr("%1 seconds").arg(Monitor.historyDuration)
                color: FishUI.Theme.disabledTextColor
                font.pointSize: 8
            }
            Item {
                Layout.fillWidth: true
            }
            FishUI.Label {
                text: control.primaryLegend
                color: control.chartColor
                font.pointSize: 8
                visible: text.length > 0
            }
            FishUI.Label {
                text: control.secondaryLegend
                color: control.secondaryColor
                font.pointSize: 8
                visible: text.length > 0
            }
            FishUI.Label {
                text: qsTr("Now")
                color: FishUI.Theme.disabledTextColor
                font.pointSize: 8
            }
        }
    }
}
