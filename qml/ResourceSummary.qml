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
    property string iconName
    property string value
    property string subtitle
    property var values: []
    property color chartColor
    property real maximum: 100
    property real scaleFloor: 1024
    property bool selected: false

    height: 96
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

            FishUI.IconItem {
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: titleLabel.capCenter - height / 2
                Layout.preferredWidth: 16
                Layout.preferredHeight: 16
                source: "qrc:/icons/" + control.iconName + ".svg"
                color: control.chartColor
            }
            // The capitals' top, not the line box, sits on the card's padding.
            FishUI.AlignedLabel {
                id: titleLabel
                Layout.fillWidth: true
                textBaseline: capHeight
                text: control.title
                font.pointSize: 9
                font.weight: Font.Medium
            }
            FishUI.AlignedLabel {
                textBaseline: titleLabel.textBaseline
                text: Monitor.ready ? control.value : "—"
                font.pointSize: 9
                font.weight: Font.Medium
            }
        }

        FishUI.Label {
            Layout.fillWidth: true
            Layout.leftMargin: 16 + 8
            text: Monitor.ready ? control.subtitle : qsTr("Collecting data…")
            color: FishUI.Theme.disabledTextColor
            font.pointSize: 8
            elide: Text.ElideRight
        }

        HistoryGraph {
            Layout.fillWidth: true
            Layout.fillHeight: true
            values: control.values
            timestamps: Monitor.history.timestamps || []
            duration: Monitor.historyDuration * 1000
            color: control.chartColor
            maximum: control.maximum
            scaleFloor: control.scaleFloor
            curved: true
            lineWidth: 1.25
        }
    }
}
