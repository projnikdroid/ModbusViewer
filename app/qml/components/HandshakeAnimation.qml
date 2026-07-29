import QtQuick

import ModbusViewer

// Shown on the connection screen while ConnectionState == Connecting (Decision 19).
// Two dots trading places suggests a request/response handshake without needing any
// imagery - kept deliberately simple for now, a real pass comes in the M8 polish
// milestone.
Item {
    id: root
    implicitWidth: 48
    implicitHeight: 16
    visible: opacity > 0
    opacity: ConnectionController.state === ConnectionController.Connecting ? 1 : 0

    Behavior on opacity {
        NumberAnimation { duration: 150 }
    }

    Rectangle {
        id: leftDot
        width: 12
        height: 12
        radius: 6
        color: Theme.accent
        anchors.verticalCenter: parent.verticalCenter

        SequentialAnimation on x {
            running: root.visible
            loops: Animation.Infinite
            NumberAnimation { from: 0; to: root.width - leftDot.width; duration: 500; easing.type: Easing.InOutQuad }
            NumberAnimation { from: root.width - leftDot.width; to: 0; duration: 500; easing.type: Easing.InOutQuad }
        }
    }

    Rectangle {
        width: 12
        height: 12
        radius: 6
        color: Theme.accentMuted
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
    }
}
