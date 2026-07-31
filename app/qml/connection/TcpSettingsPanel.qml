import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import ModbusViewer

ColumnLayout {
    spacing: Theme.spacingSm

    Label { text: "Host"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.capitalization: Font.AllUppercase; font.letterSpacing: 0.6 }
    ThemedTextField {
        Layout.fillWidth: true
        text: ConnectionController.host
        onEditingFinished: ConnectionController.host = text
    }

    Label { text: "Port"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.capitalization: Font.AllUppercase; font.letterSpacing: 0.6 }
    ThemedSpinBox {
        Layout.fillWidth: true
        from: 1
        to: 65535
        value: ConnectionController.port
        onValueModified: ConnectionController.port = value
    }

    // Absorbs the extra height the StackLayout gives this panel when the (taller)
    // RTU panel is the sibling that actually determines its size, so the fields
    // above stay their natural height instead of stretching.
    Item { Layout.fillHeight: true }
}
