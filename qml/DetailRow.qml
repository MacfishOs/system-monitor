// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import FishUI as FishUI

Item {
    id: control
    property string label
    property string value
    implicitHeight: 32

    FishUI.Label {
        id: label
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: parent.width * 0.4
        text: control.label
        textFormat: Text.PlainText
        elide: Text.ElideRight
        color: FishUI.Theme.disabledTextColor
        font.pointSize: 9
    }
    FishUI.Label {
        anchors.left: label.right
        anchors.leftMargin: 24
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        text: control.value
        textFormat: Text.PlainText
        elide: Text.ElideRight
        font.pointSize: 9
        horizontalAlignment: Text.AlignRight
    }
}
