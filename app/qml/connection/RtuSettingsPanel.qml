import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import ModbusViewer

ColumnLayout {
    id: root
    spacing: Theme.spacingSm

    // QSerialPort::Parity values, which ConnectionController.parity stores directly.
    readonly property var parityValues: [0, 2, 3, 4, 5]
    readonly property var parityLabels: ["None", "Even", "Odd", "Space", "Mark"]

    SerialPortListModel {
        id: portListModel
    }

    RowLayout {
        Layout.fillWidth: true
        Label { text: "Serial Port"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.capitalization: Font.AllUppercase; font.letterSpacing: 0.6 }
        Item { Layout.fillWidth: true }
        ThemedButton {
            text: "Rescan"
            onClicked: portListModel.rescan()
        }
    }

    ThemedComboBox {
        id: portCombo
        Layout.fillWidth: true
        model: portListModel
        textRole: "displayLabel"
        valueRole: "portName"
        enabled: count > 0
        displayText: count === 0 ? "No serial ports found" : currentText
        onActivated: ConnectionController.portName = currentValue
        onCountChanged: ConnectionController.portName = count > 0 ? currentValue : ""
        Component.onCompleted: ConnectionController.portName = count > 0 ? currentValue : ""
    }

    // Without this the only feedback for a machine with no serial hardware is a
    // disabled Connect button, which does not explain itself.
    Text {
        Layout.fillWidth: true
        visible: portCombo.count === 0
        wrapMode: Text.WordWrap
        text: "No serial ports detected. Plug in a USB-to-serial adapter (or create a "
              + "virtual COM pair with com0com) and press Rescan."
        color: Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeSm
    }

    Label { text: "Baud Rate"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.capitalization: Font.AllUppercase; font.letterSpacing: 0.6 }
    ThemedComboBox {
        Layout.fillWidth: true
        editable: true
        model: [1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200]
        currentIndex: model.indexOf(ConnectionController.baudRate)
        onAccepted: ConnectionController.baudRate = parseInt(editText)
        onActivated: ConnectionController.baudRate = model[currentIndex]
    }

    Label { text: "Data Bits"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.capitalization: Font.AllUppercase; font.letterSpacing: 0.6 }
    ThemedComboBox {
        Layout.fillWidth: true
        model: [5, 6, 7, 8]
        currentIndex: model.indexOf(ConnectionController.dataBits)
        onActivated: ConnectionController.dataBits = model[currentIndex]
    }

    Label { text: "Parity"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.capitalization: Font.AllUppercase; font.letterSpacing: 0.6 }
    ThemedComboBox {
        Layout.fillWidth: true
        model: root.parityLabels
        currentIndex: root.parityValues.indexOf(ConnectionController.parity)
        onActivated: ConnectionController.parity = root.parityValues[currentIndex]
    }

    Label { text: "Stop Bits"; color: Theme.textSecondary; font.family: Theme.fontFamily; font.capitalization: Font.AllUppercase; font.letterSpacing: 0.6 }
    ThemedComboBox {
        Layout.fillWidth: true
        model: [1, 2]
        currentIndex: model.indexOf(ConnectionController.stopBits)
        onActivated: ConnectionController.stopBits = model[currentIndex]
    }
}
