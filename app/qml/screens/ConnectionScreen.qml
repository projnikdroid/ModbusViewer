import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import ModbusViewer

Rectangle {
    id: root
    color: Theme.background

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: formColumn.implicitHeight + Theme.spacingXl * 2
        clip: true

        ColumnLayout {
            id: formColumn
            width: 380
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: Theme.spacingXl
            spacing: Theme.spacingMd

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "ModbusViewer"
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXl
                font.bold: true
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "Connect to a Modbus device"
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeMd
            }

            TabBar {
                id: connectionTypeTabs
                Layout.fillWidth: true
                currentIndex: ConnectionController.connectionType === ConnectionController.Tcp ? 0 : 1
                onCurrentIndexChanged: ConnectionController.connectionType =
                    currentIndex === 0 ? ConnectionController.Tcp : ConnectionController.Rtu

                TabButton { text: "TCP" }
                TabButton { text: "RTU (Serial)" }
            }

            StackLayout {
                Layout.fillWidth: true
                currentIndex: connectionTypeTabs.currentIndex

                TcpSettingsPanel {}
                RtuSettingsPanel {}
            }

            Label { text: "Unit ID"; color: Theme.textSecondary; font.family: Theme.fontFamily }
            SpinBox {
                Layout.fillWidth: true
                from: 1
                to: 247
                value: ConnectionController.unitId
                onValueModified: ConnectionController.unitId = value
            }

            Label { text: "Timeout (ms)"; color: Theme.textSecondary; font.family: Theme.fontFamily }
            SpinBox {
                Layout.fillWidth: true
                from: 100
                to: 30000
                stepSize: 100
                value: ConnectionController.timeoutMs
                onValueModified: ConnectionController.timeoutMs = value
            }

            Label { text: "Retry Count"; color: Theme.textSecondary; font.family: Theme.fontFamily }
            SpinBox {
                Layout.fillWidth: true
                from: 0
                to: 10
                value: ConnectionController.retryCount
                onValueModified: ConnectionController.retryCount = value
            }

            Label { text: "Reconnect Interval (ms)"; color: Theme.textSecondary; font.family: Theme.fontFamily }
            SpinBox {
                Layout.fillWidth: true
                from: 500
                to: 60000
                stepSize: 500
                value: ConnectionController.reconnectIntervalMs
                onValueModified: ConnectionController.reconnectIntervalMs = value
            }

            Rectangle {
                Layout.fillWidth: true
                visible: ConnectionController.state === ConnectionController.Failed
                color: Qt.rgba(Theme.error.r, Theme.error.g, Theme.error.b, 0.15)
                radius: Theme.radiusSm
                implicitHeight: errorLabel.implicitHeight + Theme.spacingMd

                Text {
                    id: errorLabel
                    anchors.centerIn: parent
                    width: parent.width - Theme.spacingMd
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    text: ConnectionController.errorMessage
                    color: Theme.error
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSm
                }
            }

            Button {
                Layout.fillWidth: true
                text: ConnectionController.state === ConnectionController.Connecting ? "Connecting..." : "Connect"
                enabled: ConnectionController.canConnect
                         && ConnectionController.state !== ConnectionController.Connecting
                onClicked: ConnectionController.connectToDevice()
            }

            HandshakeAnimation {
                Layout.alignment: Qt.AlignHCenter
            }
        }
    }
}
