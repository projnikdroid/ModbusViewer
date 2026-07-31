import QtQuick
import QtQuick.Controls

import ModbusViewer

// contentItem is a TextField (not a plain Text) so the one editable combo in
// this app (RtuSettingsPanel's baud-rate picker) keeps working -- mirrors
// QtQuick.Controls.Basic's own ComboBox.qml, which does the same for the
// same reason.
ComboBox {
    id: control

    font.family: Theme.fontFamily
    font.pixelSize: Theme.fontSizeMd

    background: Rectangle {
        implicitHeight: 34
        radius: Theme.radiusSm
        color: control.pressed ? Theme.surfaceRaised : Theme.surface
        border.width: 1
        border.color: control.activeFocus ? Theme.accent : Theme.border
    }

    contentItem: TextField {
        text: control.editable ? control.editText : control.displayText
        enabled: control.editable
        readOnly: !control.editable
        autoScroll: control.editable
        font: control.font
        color: control.enabled ? Theme.textPrimary : Theme.textDisabled
        selectionColor: Theme.accent
        selectedTextColor: Theme.background
        verticalAlignment: Text.AlignVCenter
        leftPadding: Theme.spacingSm
        rightPadding: control.indicator.width + Theme.spacingSm
        background: Item {}
    }

    indicator: Text {
        x: control.width - width - Theme.spacingSm
        y: (control.height - height) / 2
        text: "▾"
        color: Theme.textSecondary
        font.pixelSize: Theme.fontSizeSm
    }

    // textAt(index) delegates to ComboBox's own textRole-aware lookup rather
    // than re-deriving it by hand -- handles plain string/number array
    // models and array-of-object models with textRole set uniformly.
    delegate: ThemedItemDelegate {
        width: control.width
        text: control.textAt(index)
        font: control.font
        highlighted: control.highlightedIndex === index
    }

    popup: Popup {
        y: control.height
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight, 320)
        padding: 1

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }

        background: Rectangle {
            color: Theme.surfaceOpaque
            border.width: 1
            border.color: Theme.border
            radius: Theme.radiusSm
        }
    }
}
