import QtQuick
import QtQuick.Controls

import ModbusViewer

SpinBox {
    id: control

    font.family: Theme.fontFamilyMono
    font.pixelSize: Theme.fontSizeMd

    // Control positions contentItem inside the padding-inset content box
    // automatically; without these the default padding is far narrower than
    // the 26px indicators below, so the value text renders underneath/
    // overlapping the +/- buttons instead of between them.
    leftPadding: down.indicator ? down.indicator.width : 0
    rightPadding: up.indicator ? up.indicator.width : 0

    contentItem: TextInput {
        z: 2
        text: control.textFromValue(control.value, control.locale)
        font: control.font
        color: control.enabled ? Theme.textPrimary : Theme.textDisabled
        selectionColor: Theme.accent
        selectedTextColor: Theme.background
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
        // Every SpinBox in this app relies on direct typing and never opts
        // into `editable` (Qt Quick Controls' own default leaves typing
        // available regardless) -- gating on it here made every field in
        // this app read-only, a real regression, not a themed equivalent.
        validator: control.validator
        inputMethodHints: Qt.ImhFormattedNumbersOnly
    }

    up.indicator: Rectangle {
        x: control.mirrored ? 0 : parent.width - width
        height: parent.height
        implicitWidth: 26
        color: control.up.pressed ? Theme.accentMuted : Theme.surfaceRaised
        border.width: 1
        border.color: Theme.border

        Text {
            text: "+"
            anchors.fill: parent
            fontSizeMode: Text.Fit
            color: control.enabled ? Theme.textSecondary : Theme.textDisabled
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    down.indicator: Rectangle {
        x: control.mirrored ? parent.width - width : 0
        height: parent.height
        implicitWidth: 26
        color: control.down.pressed ? Theme.accentMuted : Theme.surfaceRaised
        border.width: 1
        border.color: Theme.border

        Text {
            text: "−"
            anchors.fill: parent
            fontSizeMode: Text.Fit
            color: control.enabled ? Theme.textSecondary : Theme.textDisabled
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    background: Rectangle {
        implicitHeight: 34
        radius: Theme.radiusSm
        color: Theme.surface
        border.width: 1
        border.color: control.activeFocus ? Theme.accent : Theme.border
    }
}
