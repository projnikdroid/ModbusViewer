import QtQuick
import QtQuick.Controls

import ModbusViewer

// Skins QtQuick Controls' default "Basic" style Button against Theme.* tokens
// so both themes get one delegate, not a set per theme. `accented` is opt-in
// (default false) -- only the couple of hero call-to-action buttons (Connect,
// Reconnect Now) set it, everything else stays a plain themed button.
Button {
    id: control

    property bool accented: false

    readonly property bool hasGradient: control.accented && Theme.accentGradientStops.length > 1

    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSizeMd

    background: Rectangle {
        implicitHeight: 34
        implicitWidth: 90
        radius: Theme.radiusSm
        border.width: control.flat ? 0 : 1
        border.color: control.accented ? Theme.accent : (control.down ? Theme.accent : Theme.border)
        color: control.flat
            ? (control.down ? Theme.surfaceRaised : "transparent")
            : (control.accented
                ? (hasGradient ? "transparent" : (control.down ? Theme.accent : Theme.accentMuted))
                : (control.down ? Theme.accentMuted : (control.hovered ? Theme.surfaceRaised : Theme.surface)))

        gradient: hasGradient && !control.down ? accentGradient : null

        readonly property Gradient accentGradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: Theme.accentGradientStops.length > 0 ? Theme.accentGradientStops[0] : Theme.accent }
            GradientStop { position: 1.0; color: Theme.accentGradientStops.length > 1 ? Theme.accentGradientStops[1] : Theme.accent }
        }
    }

    contentItem: Text {
        text: control.text
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        color: !control.enabled
            ? Theme.textDisabled
            : control.flat
                ? Theme.textSecondary
                : (control.accented ? (hasGradient ? Theme.background : Theme.accent) : Theme.textPrimary)
    }
}
