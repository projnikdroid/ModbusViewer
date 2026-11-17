import QtQuick
import QtQuick.Controls
import QtTest
import ModbusViewer

// RtuSettingsPanel.qml (the real file, against a stub ConnectionController).
//
// The baud-rate box is an editable combo. Regression coverage for a bug where a
// custom rate only took effect on Enter: typing one and going straight to Connect
// left the box showing a rate the connection wasn't using -- on a real RS-485 bus,
// indistinguishable from a silent device.
Item {
    id: root
    width: 400
    height: 700

    Component {
        id: harness
        Column {
            width: 400
            property alias panel: p
            property alias other: b

            RtuSettingsPanel {
                id: p
                width: parent.width
            }
            // Stands in for the Connect button: somewhere to move focus to.
            ThemedButton {
                id: b
                text: "Connect"
            }
        }
    }

    TestCase {
        name: "RtuSettingsPanel"
        when: windowShown

        function init() {
            ConnectionController.baudRate = 115200
            ConnectionController.parity = 0
        }

        function baudCombo(h) {
            const c = findChild(h.panel, "baudRateCombo")
            verify(c !== null, "baudRateCombo not found")
            return c
        }

        function typeIntoBaud(combo, text) {
            combo.contentItem.forceActiveFocus()
            combo.contentItem.selectAll()
            for (const ch of text)
                keyClick(Qt.Key_0 + Number(ch))
        }

        function test_customBaudCommitsOnEnter() {
            const h = createTemporaryObject(harness, root)
            const combo = baudCombo(h)
            typeIntoBaud(combo, "230400")
            keyClick(Qt.Key_Return)
            compare(ConnectionController.baudRate, 230400)
            compare(combo.editText, "230400")
        }

        // The reported bug: no Enter, straight to another control.
        function test_customBaudCommitsWhenFocusLeavesWithoutEnter() {
            const h = createTemporaryObject(harness, root)
            const combo = baudCombo(h)
            typeIntoBaud(combo, "230400")
            mouseClick(h.other)
            compare(ConnectionController.baudRate, 230400)
            compare(combo.editText, "230400")
        }

        function test_standardBaudTypedAndLeftCommits() {
            const h = createTemporaryObject(harness, root)
            const combo = baudCombo(h)
            typeIntoBaud(combo, "9600")
            mouseClick(h.other)
            compare(ConnectionController.baudRate, 9600)
            compare(combo.editText, "9600")
        }

        // An emptied box must not set the rate to NaN/0, and must not keep showing
        // nothing while the connection still uses 115200.
        function test_emptiedBaudRevertsToTheEffectiveRate() {
            const h = createTemporaryObject(harness, root)
            const combo = baudCombo(h)
            combo.contentItem.forceActiveFocus()
            combo.contentItem.selectAll()
            keyClick(Qt.Key_Backspace)
            mouseClick(h.other)
            compare(ConnectionController.baudRate, 115200)
            compare(combo.editText, "115200")
        }

        function test_nonDigitsAreRejected() {
            const h = createTemporaryObject(harness, root)
            const combo = baudCombo(h)
            combo.contentItem.forceActiveFocus()
            combo.contentItem.selectAll()
            keyClick(Qt.Key_A)
            keyClick(Qt.Key_B)
            mouseClick(h.other)
            compare(ConnectionController.baudRate, 115200)
            compare(combo.editText, "115200")
        }

        function test_focusingAndLeavingWithoutTypingChangesNothing() {
            const h = createTemporaryObject(harness, root)
            const combo = baudCombo(h)
            combo.contentItem.forceActiveFocus()
            mouseClick(h.other)
            compare(ConnectionController.baudRate, 115200)
            compare(combo.editText, "115200")
        }

        // The panel maps ConnectionController.parity (a QSerialPort::Parity int)
        // to a label; the C++ default of 0 (NoParity) must read "None".
        function test_parityComboShowsNoneForNoParityAndEvenForEven() {
            const h = createTemporaryObject(harness, root)
            const parity = findChild(h.panel, "parityCombo")
            verify(parity !== null, "parityCombo not found")
            compare(parity.currentText, "None")
            ConnectionController.parity = 2 // QSerialPort::EvenParity
            compare(parity.currentText, "Even")
        }
    }
}
