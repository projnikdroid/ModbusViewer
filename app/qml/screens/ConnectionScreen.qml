import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import ModbusViewer

Rectangle {
    id: root
    color: Theme.background

    ThemeBackdrop {
        anchors.fill: parent
    }

    RowLayout {
        // Declared before the Flickable below but must render/hit-test above
        // it -- Flickable fills the whole screen and, without this, silently
        // swallows clicks meant for this corner control since it paints
        // (and receives input) on top by declaration order alone.
        z: 1
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: Theme.spacingMd
        spacing: Theme.spacingSm

        Label { text: "Theme:"; color: Theme.textSecondary; font.family: Theme.fontFamily }
        ThemedComboBox {
            model: Theme.availableThemes
            textRole: "label"
            valueRole: "id"
            currentIndex: Math.max(0, indexOfValue(ThemeSettings.themeId))
            onActivated: ThemeSettings.themeId = currentValue
        }
    }

    // Credits watermark -- same z-stacking reasoning as the theme picker
    // above (Flickable below would otherwise paint over it). Click it for a
    // just-for-fun "warp core" pulse, playing off its own text.
    Item {
        z: 1
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: Theme.spacingSm
        width: creditsText.implicitWidth
        height: creditsText.implicitHeight

        Repeater {
            id: ringRepeater
            model: 3
            delegate: Rectangle {
                id: ring
                required property int index

                anchors.centerIn: parent
                width: 12
                height: 12
                radius: width / 2
                color: "transparent"
                border.width: 2
                border.color: Theme.accent
                opacity: 0
                scale: 1

                function pulse() { pulseAnim.restart() }

                SequentialAnimation {
                    id: pulseAnim
                    PauseAnimation { duration: ring.index * 130 }
                    ParallelAnimation {
                        NumberAnimation { target: ring; property: "scale"; from: 1; to: 7; duration: 650; easing.type: Easing.OutCubic }
                        NumberAnimation { target: ring; property: "opacity"; from: 0.85; to: 0; duration: 650; easing.type: Easing.OutCubic }
                    }
                }
            }
        }

        Text {
            id: creditsText
            anchors.centerIn: parent
            text: "// engineered by Nikhil Bangar · warp core stable"
            color: Theme.textDisabled
            font.family: Theme.fontFamilyMono
            font.pixelSize: Theme.fontSizeSm

            SequentialAnimation {
                id: textPulse
                NumberAnimation { target: creditsText; property: "scale"; from: 1; to: 1.06; duration: 150; easing.type: Easing.OutQuad }
                PauseAnimation { duration: 300 }
                NumberAnimation { target: creditsText; property: "scale"; to: 1; duration: 300; easing.type: Easing.InQuad }
            }
            ColorAnimation on color {
                id: colorPulse
                from: Theme.accent
                to: Theme.textDisabled
                duration: 900
                running: false
            }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                textPulse.restart()
                colorPulse.restart()
                for (var i = 0; i < ringRepeater.count; ++i)
                    ringRepeater.itemAt(i).pulse()
            }
        }
    }

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
                id: titleText
                Layout.alignment: Qt.AlignHCenter
                text: "ModbusViewer"
                color: Theme.accentGradientStops.length > 1 ? Theme.accentGradientStops[0] : Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeXl
                font.bold: true
            }

            // Hero accent bar under the wordmark -- QML Text can't be
            // gradient-filled without a shader/effects module this project
            // deliberately doesn't add, so the gradient lives on this thin
            // rule instead. Flat accent when the active theme has no
            // gradient stops.
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                width: titleText.implicitWidth
                height: 3
                radius: 1.5
                color: Theme.accentGradientStops.length > 1 ? "transparent" : Theme.accent
                gradient: Theme.accentGradientStops.length > 1 ? titleGradient : null

                Gradient {
                    id: titleGradient
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: Theme.accentGradientStops.length > 0 ? Theme.accentGradientStops[0] : Theme.accent }
                    GradientStop { position: 1.0; color: Theme.accentGradientStops.length > 1 ? Theme.accentGradientStops[1] : Theme.accent }
                }
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "Connect to a Modbus device"
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeMd
            }

            ThemedTabBar {
                id: connectionTypeTabs
                Layout.fillWidth: true
                currentIndex: ConnectionController.connectionType === ConnectionController.Tcp ? 0 : 1
                onCurrentIndexChanged: ConnectionController.connectionType =
                    currentIndex === 0 ? ConnectionController.Tcp : ConnectionController.Rtu

                ThemedTabButton { text: "TCP" }
                ThemedTabButton { text: "RTU (Serial)" }
            }

            StackLayout {
                Layout.fillWidth: true
                currentIndex: connectionTypeTabs.currentIndex

                TcpSettingsPanel {}
                RtuSettingsPanel {}
            }

            Label { text: "Unit ID"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.capitalization: Font.AllUppercase; font.letterSpacing: 0.6 }
            ThemedSpinBox {
                Layout.fillWidth: true
                from: 1
                to: 247
                value: ConnectionController.unitId
                onValueModified: ConnectionController.unitId = value
            }

            Label { text: "Timeout (ms)"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.capitalization: Font.AllUppercase; font.letterSpacing: 0.6 }
            ThemedSpinBox {
                Layout.fillWidth: true
                from: 100
                to: 30000
                stepSize: 100
                value: ConnectionController.timeoutMs
                onValueModified: ConnectionController.timeoutMs = value
            }

            Label { text: "Retry Count"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.capitalization: Font.AllUppercase; font.letterSpacing: 0.6 }
            ThemedSpinBox {
                Layout.fillWidth: true
                from: 0
                to: 10
                value: ConnectionController.retryCount
                onValueModified: ConnectionController.retryCount = value
            }

            Label { text: "Reconnect Interval (ms)"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.capitalization: Font.AllUppercase; font.letterSpacing: 0.6 }
            ThemedSpinBox {
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

            ThemedButton {
                Layout.fillWidth: true
                accented: true
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
