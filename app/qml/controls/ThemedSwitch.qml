import QtQuick
import QtQuick.Controls

import ModbusViewer

Switch {
    id: control

    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSizeMd

    indicator: Rectangle {
        x: control.leftPadding
        y: (control.height - height) / 2
        implicitWidth: 40
        implicitHeight: 22
        radius: height / 2
        color: control.checked ? Theme.accent : Theme.surfaceRaised
        border.width: 1
        border.color: control.checked ? Theme.accent : Theme.border

        Rectangle {
            x: control.checked ? parent.width - width - 2 : 2
            anchors.verticalCenter: parent.verticalCenter
            width: 18
            height: 18
            radius: 9
            color: control.checked ? Theme.background : Theme.textPrimary

            Behavior on x {
                NumberAnimation { duration: 120 }
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
}
