import QtQuick
import QtQuick.Controls

import ModbusViewer

// Shared themed list-row control -- used by ThemedComboBox's dropdown rows
// and by any plain list (e.g. MainScreen.qml's "Add From Tag..." search
// results) that would otherwise fall back to Basic style's default
// ItemDelegate (a light panel with dark text, badly out of place against
// either theme).
ItemDelegate {
    id: control

    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSizeMd

    contentItem: Text {
        text: control.text
        color: Theme.textPrimary
        font: control.font
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
        leftPadding: Theme.spacingSm
    }

    background: Rectangle {
        color: control.highlighted ? Theme.accentMuted : Theme.surfaceOpaque
    }
}
