import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import ModbusViewer

// Per-row display-format editor (M6). Prefilled from RegisterTableModel's
// *requested* settings (formatSettingsAt), not the possibly-degraded effective ones
// actually being rendered, so reopening a temporarily-degraded row doesn't silently
// discard what the user asked for.
Popup {
    id: root
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: Theme.spacingMd

    property var registerModel: null
    property int targetRow: -1

    readonly property var formatOptions: [
        { text: "Signed Decimal", value: 0 },
        { text: "Unsigned Decimal", value: 1 },
        { text: "Hex", value: 2 },
        { text: "Binary", value: 3 },
        { text: "Float32", value: 4 },
        { text: "Int32 (Signed)", value: 5 },
        { text: "Int32 (Unsigned)", value: 6 },
    ]
    readonly property var byteOrderOptions: [
        { text: "ABCD", value: 0 },
        { text: "BADC", value: 1 },
        { text: "CDAB", value: 2 },
        { text: "DCBA", value: 3 },
    ]
    readonly property bool isMultiRegisterFormat: formatCombo.currentValue >= 4
    readonly property bool ignoresScaleOffsetUnit: formatCombo.currentValue === 2 || formatCombo.currentValue === 3

    function openFor(row) {
        targetRow = row
        if (registerModel) {
            const settings = registerModel.formatSettingsAt(row)
            formatCombo.currentIndex = settings.format
            byteOrderCombo.currentIndex = settings.byteOrder
            scaleField.text = String(settings.scale)
            offsetField.text = String(settings.offset)
            unitField.text = settings.unit
        }
        open()
    }

    contentItem: ColumnLayout {
        spacing: Theme.spacingSm

        Label { text: "Display Format"; color: Theme.textSecondary }
        ComboBox {
            id: formatCombo
            Layout.fillWidth: true
            model: root.formatOptions
            textRole: "text"
            valueRole: "value"
        }

        Label {
            text: "Byte Order"
            color: Theme.textSecondary
            visible: root.isMultiRegisterFormat
        }
        ComboBox {
            id: byteOrderCombo
            Layout.fillWidth: true
            visible: root.isMultiRegisterFormat
            model: root.byteOrderOptions
            textRole: "text"
            valueRole: "value"
        }

        RowLayout {
            spacing: Theme.spacingSm
            enabled: !root.ignoresScaleOffsetUnit

            Label { text: "Scale"; color: Theme.textSecondary }
            TextField { id: scaleField; Layout.preferredWidth: 70 }

            Label { text: "Offset"; color: Theme.textSecondary }
            TextField { id: offsetField; Layout.preferredWidth: 70 }

            Label { text: "Unit"; color: Theme.textSecondary }
            TextField { id: unitField; Layout.preferredWidth: 70 }
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: Theme.spacingSm

            Button {
                text: "Cancel"
                flat: true
                onClicked: root.close()
            }
            Button {
                text: "OK"
                onClicked: {
                    if (root.registerModel && root.targetRow >= 0) {
                        const parsedScale = parseFloat(scaleField.text)
                        const parsedOffset = parseFloat(offsetField.text)
                        root.registerModel.setFormatAt(
                            root.targetRow,
                            formatCombo.currentValue,
                            byteOrderCombo.currentValue,
                            isNaN(parsedScale) ? 1 : parsedScale,
                            isNaN(parsedOffset) ? 0 : parsedOffset,
                            unitField.text)
                    }
                    root.close()
                }
            }
        }
    }
}
