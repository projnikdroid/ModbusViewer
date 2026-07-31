import QtQuick
import QtQuick.Controls

import ModbusViewer

CheckBox {
    id: control

    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSizeMd

    indicator: Rectangle {
        x: control.leftPadding
        y: (control.height - height) / 2
        implicitWidth: 18
        implicitHeight: 18
        radius: Math.min(Theme.radiusSm, 4)
        color: control.checked ? Theme.accent : Theme.surface
        border.width: 1
        border.color: control.checked ? Theme.accent : Theme.border

        // Drawn, not a "✓" glyph -- some fonts render certain check-mark
        // characters with their own baked-in emoji-style box/background
        // depending on font fallback, which then visibly doubled up with
        // this Rectangle's own box. A hand-drawn stroke sidesteps font
        // rendering entirely.
        Canvas {
            id: checkMark
            anchors.centerIn: parent
            width: 10
            height: 8
            visible: control.checked

            Component.onCompleted: requestPaint()
            onVisibleChanged: if (visible) requestPaint()

            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()
                ctx.strokeStyle = Theme.background
                ctx.lineWidth = 2
                ctx.lineCap = "round"
                ctx.lineJoin = "round"
                ctx.beginPath()
                ctx.moveTo(0, 4)
                ctx.lineTo(3.5, 7.5)
                ctx.lineTo(10, 0.5)
                ctx.stroke()
            }

            Connections {
                target: Theme
                function onBackgroundChanged() { checkMark.requestPaint() }
            }
        }
    }

    contentItem: Text {
        text: control.text
        font: control.font
        color: Theme.textSecondary
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
    }

    // Only indicator/contentItem were themed before -- Basic style's own
    // default background (a hover/press highlight box) was still showing
    // through underneath, which is the most likely cause of a second
    // check-shaped visual appearing on hover.
    background: Item {}
}
