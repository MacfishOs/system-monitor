// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import FishUI as FishUI

AbstractButton {
    id: control
    property string column
    property bool numeric: true
    height: 30
    hoverEnabled: true
    onClicked: Monitor.processes.sortBy(column)
    Accessible.name: qsTr("Sort by %1").arg(text)

    background: FishUI.RoundedRectangle {
        radius: 6
        color: control.hovered ? Qt.alpha(FishUI.Theme.textColor, 0.04) : "transparent"
    }

    contentItem: Item {
        FishUI.Label {
            id: label
            x: control.numeric ? parent.width - width - 12 : 12
            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            font.pointSize: 8
            font.weight: Font.Medium
            color: FishUI.Theme.disabledTextColor
        }

        FishUI.Icon {
            x: control.numeric ? label.x - 16 : label.x + label.width + 6
            anchors.verticalCenter: parent.verticalCenter
            width: 10
            height: 10
            name: Monitor.processes.ascending ? "chevron-up" : "chevron-down"
            visible: Monitor.processes.sortColumn === control.column
            color: FishUI.Theme.disabledTextColor
        }
    }
}
