import QtQuick
import QtQuick.Controls

import ModbusViewer

TextField {
    id: control

    font.family: Theme.fontFamilyMono
    font.pixelSize: Theme.fontSizeMd
    color: control.enabled ? Theme.textPrimary : Theme.textDisabled
    selectionColor: Theme.accent
    selectedTextColor: Theme.background
    placeholderTextColor: Theme.textDisabled
    leftPadding: Theme.spacingSm
    rightPadding: Theme.spacingSm

    background: Rectangle {
        implicitHeight: 34
        radius: Theme.radiusSm
        color: Theme.surface
        border.width: 1
        border.color: control.activeFocus ? Theme.accent : Theme.border
    }
}
