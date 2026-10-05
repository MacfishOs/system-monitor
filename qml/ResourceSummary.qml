// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FishUI as FishUI
import Cutefish.SystemMonitor

AbstractButton {
    id: control
    property string title
    property string value
    property string subtitle
    property var values: []
    property var secondaryValues: []
    property color chartColor
    property color secondaryColor: chartColor
    property real maximum: 100
    property bool selected: false

    height: 112
    padding: 12
    hoverEnabled: true
    Accessible.name: title + " " + value + " " + subtitle

    background: FishUI.RoundedRectangle {
        radius: FishUI.Theme.mediumRadius
        color: control.selected ? Qt.alpha(control.chartColor, FishUI.Theme.darkMode ? 0.15 : 0.09)
                                : control.hovered ? Qt.tint(FishUI.Theme.secondBackgroundColor, Qt.alpha(FishUI.Theme.textColor, 0.025))
                                                  : FishUI.Theme.secondBackgroundColor
        border.width: control.selected || control.visualFocus ? 1 / FishUI.Dpi.ratio : 0
        border.color: Qt.alpha(control.chartColor, 0.5)
    }

    contentItem: ColumnLayout {
        spacing: 5

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Rectangle {
                Layout.preferredWidth: 6
                Layout.preferredHeight: 6
                radius: 3
                color: control.chartColor
            }
            FishUI.Label {
                Layout.fillWidth: true
                text: control.title
                font.pointSize: 9
                font.weight: Font.Medium
            }
            FishUI.Label {
                text: Monitor.ready ? control.value : "—"
                font.pointSize: 9
                font.weight: Font.Medium
            }
        }

        FishUI.Label {
            Layout.fillWidth: true
            text: Monitor.ready ? control.subtitle : qsTr("Collecting data…")
            color: FishUI.Theme.disabledTextColor
            font.pointSize: 8
            elide: Text.ElideRight
        }

        HistoryGraph {
            Layout.fillWidth: true
            Layout.fillHeight: true
            values: control.values
            secondaryValues: control.secondaryValues
            timestamps: Monitor.history.timestamps || []
            duration: Monitor.historyDuration * 1000
            color: control.chartColor
            secondaryColor: control.secondaryColor
            maximum: control.maximum
            gridColor: Qt.alpha(FishUI.Theme.textColor, 0.06)
        }
    }
}
