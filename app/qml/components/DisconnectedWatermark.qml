import QtQuick
import QtQuick.Controls

import ModbusViewer

// Full-screen overlay for a hard transport loss (Decision 21). Sits over the last
// polled values rather than replacing them - the table underneath keeps showing
// whatever it last had, dimmed, so the user has more context than a blank screen
// while a reconnect is attempted.
Rectangle {
    id: root
    anchors.fill: parent
    color: Qt.rgba(0, 0, 0, 0.55)
    visible: opacity > 0
    opacity: ConnectionController.state === ConnectionController.ConnectionLost ? 1 : 0

    Behavior on opacity {
        NumberAnimation { duration: 200 }
    }

    // Consume input so nothing underneath is editable while disconnected.
    MouseArea {
        anchors.fill: parent
        onClicked: {}
        onWheel: (wheel) => wheel.accepted = true
    }

    Column {
        anchors.centerIn: parent
        spacing: Theme.spacingMd

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "DISCONNECTED"
            color: Theme.error
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeXl
            font.bold: true
            font.letterSpacing: 4
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Showing last known values. Reconnecting..."
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeMd
        }

        ThemedButton {
            anchors.horizontalCenter: parent.horizontalCenter
            accented: true
            text: "Reconnect Now"
            onClicked: ConnectionController.connectToDevice()
        }

        ThemedButton {
            anchors.horizontalCenter: parent.horizontalCenter
            flat: true
            text: "Give Up"
            onClicked: ConnectionController.disconnectFromDevice()
        }
    }
}
