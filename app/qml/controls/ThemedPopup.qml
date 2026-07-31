import QtQuick
import QtQuick.Controls

import ModbusViewer

// Not one of the 7 control types the redesign originally scoped, but
// FormatPicker.qml's root is a Popup -- leaving it in the default Basic
// style would put an unthemed white panel in the middle of an otherwise
// fully-skinned screen, so it gets the same treatment.
Popup {
    id: control

    // Bindings, not one-time assignments, so the popup re-centers as the
    // window is resized/maximized rather than staying pinned to wherever it
    // first opened.
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0

    background: Rectangle {
        // Opaque, not the translucent Theme.surface used on in-page panels --
        // a floating Popup has no guaranteed dark backdrop behind it to
        // blend against.
        color: Theme.surfaceOpaque
        radius: Theme.radiusMd
        border.width: 1
        border.color: Theme.border
    }

    Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, 0.5)
    }
}
