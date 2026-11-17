import QtQuick
import QtQuick.Controls
import QtTest
import ModbusViewer

// ThemedSpinBox: typed values must actually take effect.
//
// Regression coverage for a bug where typing into any SpinBox in the app only
// *displayed* the text -- Qt applies typed text to `value` only when `editable` is
// true -- so the next +/- (or any binding re-evaluation) snapped the field back to
// its old value (typing 41080 into Start Address, then clicking +, went to 40001).
Item {
    id: root
    width: 400
    height: 200

    // Same shape as MainScreen.qml's Start Address field: the displayed value is
    // bound to a model value, and written back on valueModified.
    QtObject {
        id: addressModel
        property int displayAddress: 40001
    }

    Component {
        id: addressField
        Column {
            property alias spin: s
            property alias other: b
            property int valueAtClick: -1
            property int modifiedCount: 0

            ThemedSpinBox {
                id: s
                from: 40001
                to: 465536
                value: addressModel.displayAddress
                onValueModified: {
                    addressModel.displayAddress = value
                    parent.modifiedCount++
                }
            }
            // Stands in for the Read button: the Quantity and ad-hoc address
            // fields are read at *click* time with no Enter pressed.
            ThemedButton {
                id: b
                text: "Read"
                onClicked: parent.valueAtClick = s.value
            }
        }
    }

    Component {
        id: smallRangeField
        ThemedSpinBox {
            from: 1
            to: 125
            value: 10
        }
    }

    TestCase {
        name: "ThemedSpinBox"
        when: windowShown

        function init() {
            addressModel.displayAddress = 40001
        }

        function typeAndSelectAll(spin, digits) {
            spin.forceActiveFocus()
            spin.contentItem.selectAll()
            for (const ch of digits)
                keyClick(Qt.Key_0 + Number(ch))
        }

        function test_isEditable() {
            const w = createTemporaryObject(addressField, root)
            compare(w.spin.editable, true)
            compare(w.spin.contentItem.readOnly, false)
        }

        function test_typedValueCommitsOnEnter() {
            const w = createTemporaryObject(addressField, root)
            typeAndSelectAll(w.spin, "41080")
            keyClick(Qt.Key_Return)
            compare(w.spin.value, 41080)
            compare(addressModel.displayAddress, 41080)
            compare(w.modifiedCount, 1)
        }

        // The reported bug: after typing 41080 + Enter, pressing + used to jump
        // back to 40001 instead of continuing from the typed value.
        function test_plusAndMinusContinueFromTheTypedValue() {
            const w = createTemporaryObject(addressField, root)
            typeAndSelectAll(w.spin, "41080")
            keyClick(Qt.Key_Return)

            mouseClick(w.spin.up.indicator)
            compare(w.spin.value, 41081)
            mouseClick(w.spin.down.indicator)
            mouseClick(w.spin.down.indicator)
            compare(w.spin.value, 41079)
        }

        // Quantity / ad-hoc address: read when a button is clicked, with no Enter.
        function test_typedValueCommitsWhenClickingAnotherControl() {
            const w = createTemporaryObject(addressField, root)
            typeAndSelectAll(w.spin, "41080")
            mouseClick(w.other)
            compare(w.valueAtClick, 41080)
        }

        function test_typedValueCommitsOnTab() {
            const w = createTemporaryObject(addressField, root)
            typeAndSelectAll(w.spin, "41080")
            keyClick(Qt.Key_Tab)
            compare(w.spin.value, 41080)
            compare(w.modifiedCount, 1)
        }

        function test_outOfRangeValueIsClampedNotIgnored() {
            const w = createTemporaryObject(smallRangeField, root)
            typeAndSelectAll(w, "999") // max is 125
            keyClick(Qt.Key_Return)
            compare(w.value, 125)
        }

        // editable makes the field parse its own *displayed* text on focus loss, so
        // focusing and leaving without typing must be a no-op -- it must not
        // re-parse a formatted value (thousands separators and all) into
        // something different, nor report a modification.
        function test_focusingAndLeavingWithoutTypingChangesNothing_data() {
            return [
                { tag: "startAddressModicon", from: 40001, to: 465536, value: 41080 },
                { tag: "startAddressPdu",     from: 0,     to: 65535,  value: 1079 },
                { tag: "pollInterval",        from: 10,    to: 60000,  value: 1000 },
                { tag: "timeout",             from: 100,   to: 30000,  value: 1000 },
                { tag: "reconnectInterval",   from: 500,   to: 60000,  value: 5000 },
                { tag: "tcpPort",             from: 1,     to: 65535,  value: 1502 },
                { tag: "unitId",              from: 1,     to: 247,    value: 17 }
            ]
        }
        function test_focusingAndLeavingWithoutTypingChangesNothing(data) {
            const w = createTemporaryObject(addressField, root)
            w.spin.from = data.from
            w.spin.to = data.to
            w.spin.value = data.value
            w.spin.contentItem.forceActiveFocus()
            keyClick(Qt.Key_Tab)
            compare(w.spin.value, data.value)
            compare(w.modifiedCount, 0)
        }
    }
}
