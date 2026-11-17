pragma Singleton
import QtQuick

// Test stand-in for the C++ ConnectionController singleton: just the serial-settings
// properties RtuSettingsPanel.qml reads and writes, with the same defaults. Tests
// reset these in init() since a singleton is shared across test functions.
QtObject {
    property string portName: ""
    property int baudRate: 115200
    property int dataBits: 8
    property int parity: 0 // QSerialPort::NoParity
    property int stopBits: 1
}
