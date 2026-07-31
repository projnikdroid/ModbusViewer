import QtQuick

import ModbusViewer

// Purely decorative, static per active theme -- repainted on resize or theme
// change, never per frame. Glass HUD's radial wash needs Canvas 2D's
// createRadialGradient() (QML's own Rectangle.gradient is linear-only);
// Signal Console's grid is a plain hairline stroke pattern. Same Canvas
// technique as MainScreen.qml's M10c sparkline -- no new Qt module needed.
Canvas {
    id: root

    Connections {
        target: Theme
        function onBlurEnabledChanged() { root.requestPaint() }
        function onGridBackgroundEnabledChanged() { root.requestPaint() }
    }

    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    onPaint: {
        var ctx = getContext("2d")
        ctx.clearRect(0, 0, width, height)

        if (Theme.blurEnabled) {
            var topWash = ctx.createRadialGradient(width * 0.5, height * -0.1, 0, width * 0.5, height * -0.1, width * 0.7)
            topWash.addColorStop(0, "rgba(129,140,248,0.22)")
            topWash.addColorStop(1, "rgba(129,140,248,0)")
            ctx.fillStyle = topWash
            ctx.fillRect(0, 0, width, height)

            var cornerWash = ctx.createRadialGradient(width, height, 0, width, height, width * 0.55)
            cornerWash.addColorStop(0, "rgba(94,234,212,0.10)")
            cornerWash.addColorStop(1, "rgba(94,234,212,0)")
            ctx.fillStyle = cornerWash
            ctx.fillRect(0, 0, width, height)
        } else if (Theme.gridBackgroundEnabled) {
            ctx.strokeStyle = "rgba(255,176,0,0.05)"
            ctx.lineWidth = 1
            var step = 24
            for (var x = 0; x < width; x += step) {
                ctx.beginPath()
                ctx.moveTo(x + 0.5, 0)
                ctx.lineTo(x + 0.5, height)
                ctx.stroke()
            }
            for (var y = 0; y < height; y += step) {
                ctx.beginPath()
                ctx.moveTo(0, y + 0.5)
                ctx.lineTo(width, y + 0.5)
                ctx.stroke()
            }
        }
    }
}
