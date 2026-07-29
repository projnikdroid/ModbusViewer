import QtQuick
import QtQuick.Controls

import ModbusViewer

ApplicationWindow {
    id: window

    width: 1100
    height: 720
    minimumWidth: 720
    minimumHeight: 480
    visible: true
    title: "ModbusViewer"
    color: Theme.background

    StackView {
        id: navigationStack
        anchors.fill: parent
        initialItem: ConnectionScreen {}
    }

    Connections {
        target: ConnectionController
        function onStateChanged() {
            if (ConnectionController.state === ConnectionController.Connected && navigationStack.depth === 1) {
                navigationStack.push(Qt.resolvedUrl("screens/MainScreen.qml"))
            } else if (ConnectionController.state === ConnectionController.Disconnected
                       && navigationStack.depth > 1) {
                navigationStack.pop()
            }
        }
    }
}
