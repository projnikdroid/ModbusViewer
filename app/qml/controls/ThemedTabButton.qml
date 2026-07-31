import QtQuick
import QtQuick.Controls

import ModbusViewer

TabButton {
    id: control

    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSizeSm

    background: Rectangle {
        radius: Math.max(Theme.radiusSm - 2, 0)
        color: control.checked ? Theme.accentMuted : "transparent"
        border.width: control.checked ? 1 : 0
        border.color: Theme.accent
    }

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.checked ? Theme.textPrimary : Theme.textSecondary
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
