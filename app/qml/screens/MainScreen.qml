import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import ModbusViewer

Rectangle {
    id: root
    color: Theme.background

    // The PDU (wire) address is the source of truth; startAddressField only ever
    // shows/accepts it transformed through the active DisplaySettings convention.
    property int startAddressPdu: 0

    property bool logPanelVisible: false

    // Bridges the two distinct QML-facing AddressConvention enums (RegisterTableModel's
    // own, and DisplaySettings' global one) via their shared underlying int values.
    RegisterTableModel {
        id: registerModel
        addressConvention: DisplaySettings.addressConvention
        onWriteRequested: (address, value) => ConnectionController.writeSingleRegister(address, value)
    }

    FormatPicker {
        id: formatPicker
        registerModel: registerModel
    }

    // Reuses FormatPicker.qml unchanged -- it's written against a duck-typed
    // registerModel property calling formatSettingsAt/setFormatAt, and FavoritesModel
    // implements that same contract.
    FormatPicker {
        id: favoritesFormatPicker
        registerModel: favoritesModel
    }

    FavoritesModel {
        id: favoritesModel
        addressConvention: DisplaySettings.addressConvention
        onWriteRequested: (address, value) => ConnectionController.writeSingleRegister(address, value)
    }

    TagDatabaseModel {
        id: tagDatabaseModel
    }

    ImportTagFileDialog {
        id: importTagDialog
        tagDatabaseModel: tagDatabaseModel
    }

    // One filter class, reused across the three lists despite their differing role
    // sets -- RegisterFilterProxyModel looks up label/description/address/unit by
    // name, skipping whichever a given source model doesn't expose.
    RegisterFilterProxyModel {
        id: normalFilterProxy
        sourceModel: registerModel
    }

    RegisterFilterProxyModel {
        id: favoritesFilterProxy
        sourceModel: favoritesModel
    }

    RegisterFilterProxyModel {
        id: tagFilterProxy
        sourceModel: tagDatabaseModel
    }

    CommunicationLogModel {
        id: communicationLogModel
    }

    Connections {
        target: ConnectionController
        function onHoldingRegistersRead(startAddress, values) {
            registerModel.setRegisters(startAddress, values)
            statusLabel.text = "Read " + values.length + " register(s) starting at " + startAddress
        }
        function onOperationFailed(message) {
            statusLabel.text = "Error: " + message
        }
        function onSingleRegisterWritten(address, value) {
            statusLabel.text = "Wrote " + value + " to register " + address
            ConnectionController.readHoldingRegisters(root.startAddressPdu, quantityField.value)
        }
        function onCommunicationLogged(direction, summary) {
            communicationLogModel.append(direction, summary)
        }
        function onRegisterReadFailed(reason) {
            registerModel.markStale()
        }
    }

    Connections {
        target: TagDatabaseController
        function onImportFinished(count, errors) {
            statusLabel.text = "Imported " + count + " tag(s)"
                + (errors.length > 0 ? " (" + errors.length + " error(s), see log)" : "")
        }
    }

    // Toggling mode while polling immediately retargets the single shared
    // PollEngine -- PollModeController.mode: 0 = Normal, 1 = Favorites (matches
    // DisplaySettings.addressConvention's existing int-comparison convention rather
    // than symbolic enum access).
    Connections {
        target: PollModeController
        function onModeChanged() {
            if (!ConnectionController.polling)
                return
            if (PollModeController.mode === 1)
                ConnectionController.startPollingFavorites(favoritesModel)
            else
                ConnectionController.startPolling(root.startAddressPdu, quantityField.value)
        }
    }

    Popup {
        id: addFromTagPopup
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: 320
        height: 360
        padding: Theme.spacingMd

        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.spacingSm

            TextField {
                Layout.fillWidth: true
                placeholderText: "Search tags..."
                onTextChanged: tagFilterProxy.filterText = text
            }

            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: tagFilterProxy

                delegate: ItemDelegate {
                    id: tagDelegateRoot
                    required property int index
                    required property string label
                    required property string address

                    width: ListView.view.width
                    text: tagDelegateRoot.label + " (" + tagDelegateRoot.address + ")"
                    onClicked: {
                        // addFromTag reads via TagDatabaseModel::tagAt() against the
                        // real source model, so the proxy row must be mapped back to
                        // a source row before the call, not passed through as-is.
                        favoritesModel.addFromTag(tagDatabaseModel, tagFilterProxy.mapRowToSource(tagDelegateRoot.index))
                        addFromTagPopup.close()
                    }
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingLg
        spacing: Theme.spacingMd

        RowLayout {
            spacing: Theme.spacingMd

            Text {
                text: "ModbusViewer"
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeLg
                font.bold: true
            }

            Item { Layout.fillWidth: true }

            Label { text: "Address:"; color: Theme.textSecondary }
            ComboBox {
                model: ["0-based (PDU)", "Modicon (4xxxx)"]
                currentIndex: DisplaySettings.addressConvention
                onActivated: (index) => DisplaySettings.addressConvention = index
            }

            Label { text: "Mode:"; color: Theme.textSecondary }
            ComboBox {
                model: ["Normal", "Favorites"]
                currentIndex: PollModeController.mode
                onActivated: (index) => PollModeController.mode = index
            }

            CheckBox {
                text: "Flash on update"
                checked: DisplaySettings.flashOnUpdateEnabled
                onToggled: DisplaySettings.flashOnUpdateEnabled = checked
            }

            Label { text: "Search:"; color: Theme.textSecondary }
            TextField {
                placeholderText: "label, description, address, unit"
                Layout.preferredWidth: 220
                onTextChanged: {
                    normalFilterProxy.filterText = text
                    favoritesFilterProxy.filterText = text
                }
            }

            Button {
                text: "Import Tags..."
                onClicked: importTagDialog.open()
            }

            Button {
                text: root.logPanelVisible ? "Hide Log" : "Show Log"
                onClicked: root.logPanelVisible = !root.logPanelVisible
            }

            Button {
                text: "Disconnect"
                onClicked: ConnectionController.disconnectFromDevice()
            }
        }

        RowLayout {
            spacing: Theme.spacingSm

            Label { text: "Start Address"; color: Theme.textSecondary }
            SpinBox {
                id: startAddressField
                // Q_INVOKABLE calls aren't tracked as binding dependencies by QML, so
                // each expression reads addressConvention first (comma operator) to
                // force re-evaluation when the toggle changes -- a bare method call
                // wouldn't refresh these on its own.
                from: (DisplaySettings.addressConvention, DisplaySettings.toDisplayAddress(0))
                to: (DisplaySettings.addressConvention, DisplaySettings.toDisplayAddress(65535))
                value: (DisplaySettings.addressConvention, DisplaySettings.toDisplayAddress(root.startAddressPdu))
                onValueModified: root.startAddressPdu = DisplaySettings.toPduAddress(value)
            }

            Label { text: "Quantity"; color: Theme.textSecondary }
            SpinBox {
                id: quantityField
                from: 1
                to: 125
                value: 10
            }

            Button {
                text: "Read"
                enabled: !ConnectionController.polling
                onClicked: ConnectionController.readHoldingRegisters(root.startAddressPdu, quantityField.value)
            }

            ToolSeparator {}

            Label { text: "Interval (ms)"; color: Theme.textSecondary }
            SpinBox {
                id: intervalField
                from: 10
                to: 60000
                stepSize: 100
                value: ConnectionController.pollIntervalMs
                onValueModified: ConnectionController.pollIntervalMs = value
            }

            Button {
                text: ConnectionController.polling ? "Stop Polling" : "Start Polling"
                onClicked: {
                    if (ConnectionController.polling) {
                        ConnectionController.stopPolling()
                    } else if (PollModeController.mode === 1) {
                        ConnectionController.startPollingFavorites(favoritesModel)
                    } else {
                        ConnectionController.startPolling(root.startAddressPdu, quantityField.value)
                    }
                }
            }
        }

        Text {
            id: statusLabel
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeSm
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.surface
            radius: Theme.radiusMd
            border.color: Theme.border
            clip: true

            StackLayout {
                anchors.fill: parent
                anchors.margins: 1
                currentIndex: PollModeController.mode

                TableView {
                    id: registerTableView
                    clip: true
                    model: normalFilterProxy

                    columnWidthProvider: function (column) { return column === 0 ? 90 : width - 90 }
                    rowHeightProvider: function (row) { return 40 }

                    Component.onCompleted: forceLayout()
                    onWidthChanged: forceLayout()

                    delegate: Rectangle {
                        id: delegateRoot
                        required property int row
                        required property int column
                        required property string address
                        required property string value

                        implicitWidth: registerTableView.columnWidthProvider(column)
                        implicitHeight: 40
                        color: row % 2 === 0 ? Theme.surface : Theme.surfaceRaised
                        // Triggered off the model role, not the TextField's own text
                        // (which also changes on every keystroke while editing) -- this
                        // only fires on a genuine external update.
                        onValueChanged: if (DisplaySettings.flashOnUpdateEnabled) flashAnimation.restart()

                        // A brief background tint on update, declared first so it renders
                        // behind the row's own content rather than covering it.
                        Rectangle {
                            anchors.fill: parent
                            color: Theme.accent
                            opacity: 0
                            NumberAnimation on opacity {
                                id: flashAnimation
                                running: false
                                from: 0.35
                                to: 0
                                duration: 400
                            }
                        }

                        Text {
                            visible: delegateRoot.column === 0
                            anchors.fill: parent
                            anchors.margins: Theme.spacingSm
                            text: delegateRoot.address
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            verticalAlignment: Text.AlignVCenter
                        }

                        RowLayout {
                            visible: delegateRoot.column === 1
                            anchors.fill: parent
                            anchors.margins: Theme.spacingSm
                            spacing: Theme.spacingMd

                            TextField {
                                id: valueField
                                Layout.fillWidth: true
                                // registerModel.stale is whole-range: Normal mode is
                                // always one PollTarget covering the entire visible
                                // range, so a poll failure flags every value cell
                                // (contrast Favorites' per-row staleness below).
                                // Plain black, not Theme.textPrimary -- TextField's own
                                // control background is light (Qt Quick Controls' default
                                // style), unlike the dark custom Rectangles Theme.textPrimary
                                // is designed to sit on, so the light theme text color was
                                // nearly invisible here.
                                color: registerModel.stale ? Theme.warning : "black"
                                font.bold: registerModel.stale
                                horizontalAlignment: Text.AlignRight
                                // delegateRoot.row is the proxy's row; RegisterTableModel
                                // expects the source row, hence the mapRowToSource().
                                onEditingFinished: registerModel.setValueAt(normalFilterProxy.mapRowToSource(delegateRoot.row), text)

                                // Keeps syncing from the model except while the user has
                                // the field focused, so an incoming poll update can't
                                // overwrite an in-progress edit.
                                Binding {
                                    target: valueField
                                    property: "text"
                                    value: delegateRoot.value
                                    when: !valueField.activeFocus
                                }
                            }

                            Button {
                                text: "⚙"
                                flat: true
                                Layout.preferredWidth: 32
                                onClicked: formatPicker.openFor(normalFilterProxy.mapRowToSource(delegateRoot.row))
                            }
                        }
                    }
                }

                ColumnLayout {
                    spacing: Theme.spacingSm

                    RowLayout {
                        Layout.margins: Theme.spacingSm
                        spacing: Theme.spacingSm

                        ComboBox {
                            id: adhocRegisterTypeCombo
                            // Coil/DiscreteInput are out of scope for v1: no bit-value
                            // formatting path exists anywhere in the app yet (see M6c
                            // plan notes) -- values match Core::RegisterType's ordering.
                            model: [
                                { text: "Holding Register", value: 2 },
                                { text: "Input Register", value: 3 }
                            ]
                            textRole: "text"
                            valueRole: "value"
                        }

                        SpinBox {
                            id: adhocAddressField
                            from: 0
                            to: 65535
                        }

                        Button {
                            text: "Add Ad-hoc"
                            onClicked: favoritesModel.addAdHoc(adhocRegisterTypeCombo.currentValue, adhocAddressField.value)
                        }

                        Button {
                            text: "Add From Tag..."
                            onClicked: addFromTagPopup.open()
                        }

                        Item { Layout.fillWidth: true }
                    }

                    ListView {
                        id: favoritesListView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: favoritesFilterProxy

                        delegate: Rectangle {
                            id: favDelegateRoot
                            required property int index
                            required property string label
                            required property string address
                            required property string value
                            required property bool stale

                            width: favoritesListView.width
                            height: 40
                            color: index % 2 === 0 ? Theme.surface : Theme.surfaceRaised
                            onValueChanged: if (DisplaySettings.flashOnUpdateEnabled) favFlashAnimation.restart()

                            // A brief background tint on update, declared first so it
                            // renders behind the row's own content rather than covering it.
                            Rectangle {
                                anchors.fill: parent
                                color: Theme.accent
                                opacity: 0
                                NumberAnimation on opacity {
                                    id: favFlashAnimation
                                    running: false
                                    from: 0.35
                                    to: 0
                                    duration: 400
                                }
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: Theme.spacingSm
                                spacing: Theme.spacingMd

                                Text {
                                    text: favDelegateRoot.label + " (" + favDelegateRoot.address + ")"
                                    color: Theme.textSecondary
                                    font.family: Theme.fontFamily
                                    elide: Text.ElideRight
                                    Layout.preferredWidth: 160
                                }

                                TextField {
                                    id: favValueField
                                    Layout.fillWidth: true
                                    // Per-row, unlike Normal's whole-range staleness: each
                                    // Favorites entry is its own independent PollTarget.
                                    // Plain black, not Theme.textPrimary -- see the same
                                    // note on the Normal-view value field above.
                                    color: favDelegateRoot.stale ? Theme.warning : "black"
                                    font.bold: favDelegateRoot.stale
                                    horizontalAlignment: Text.AlignRight
                                    // favDelegateRoot.index is the proxy's row; FavoritesModel
                                    // expects the source row, hence the mapRowToSource().
                                    onEditingFinished: favoritesModel.setValueAt(favoritesFilterProxy.mapRowToSource(favDelegateRoot.index), text)

                                    Binding {
                                        target: favValueField
                                        property: "text"
                                        value: favDelegateRoot.value
                                        when: !favValueField.activeFocus
                                    }
                                }

                                Button {
                                    text: "⚙"
                                    flat: true
                                    Layout.preferredWidth: 32
                                    onClicked: favoritesFormatPicker.openFor(favoritesFilterProxy.mapRowToSource(favDelegateRoot.index))
                                }

                                Button {
                                    text: "✕"
                                    flat: true
                                    Layout.preferredWidth: 32
                                    onClicked: favoritesModel.removeAt(favoritesFilterProxy.mapRowToSource(favDelegateRoot.index))
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            visible: root.logPanelVisible
            Layout.fillWidth: true
            Layout.preferredHeight: 160
            color: Theme.surface
            radius: Theme.radiusMd
            border.color: Theme.border
            clip: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingSm
                spacing: Theme.spacingSm

                RowLayout {
                    spacing: Theme.spacingSm

                    Text {
                        text: "Communication Log"
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.bold: true
                    }

                    Item { Layout.fillWidth: true }

                    Button {
                        text: "Clear"
                        flat: true
                        onClicked: communicationLogModel.clear()
                    }
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: communicationLogModel
                    // New entries are appended at the end of the model, so they
                    // render at the bottom -- follow them there automatically
                    // rather than requiring the user to scroll down manually.
                    onCountChanged: positionViewAtEnd()

                    delegate: RowLayout {
                        id: logDelegateRoot
                        required property string timestamp
                        required property int direction
                        required property string summary

                        width: ListView.view.width
                        spacing: Theme.spacingSm

                        Text {
                            text: logDelegateRoot.timestamp
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                            Layout.preferredWidth: 90
                        }

                        Text {
                            // CommunicationLogModel::Direction: 0=Tx, 1=Rx, 2=Error.
                            text: logDelegateRoot.direction === 0 ? "Tx" : logDelegateRoot.direction === 1 ? "Rx" : "Err"
                            color: logDelegateRoot.direction === 2 ? Theme.error : Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                            font.bold: true
                            Layout.preferredWidth: 30
                        }

                        Text {
                            text: logDelegateRoot.summary
                            color: logDelegateRoot.direction === 2 ? Theme.error : Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSm
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
    }

    DisconnectedWatermark {}
}
