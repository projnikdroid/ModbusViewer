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

    // The PDU (wire) address is the source of truth; startAddressField only ever
    // shows/accepts it transformed through the active DisplaySettings convention.
    property int startAddressPdu: 0

    property bool logPanelVisible: false

    // FavoritesModel's poll targets are only built at startPollingFavorites()
    // time, not automatically on every add/remove -- without this, adding an
    // entry while already polling in Favorites mode would silently never get
    // polled until the user manually stops/restarts. Call after every mutation
    // that changes which rows exist (not needed for in-place edits like
    // setBitAt/setValueAt/setFormatAt, which don't change the target set).
    function retargetFavoritesPollingIfActive() {
        if (ConnectionController.polling && PollModeController.mode === 1)
            ConnectionController.startPollingFavorites(favoritesModel)
    }

    // Bridges the two distinct QML-facing AddressConvention enums (RegisterTableModel's
    // own, and DisplaySettings' global one) via their shared underlying int values.
    RegisterTableModel {
        id: registerModel
        addressConvention: DisplaySettings.addressConvention
        onWriteRequested: (address, value) => ConnectionController.writeSingleRegister(address, value)
        onCoilWriteRequested: (address, value) => ConnectionController.writeSingleCoil(address, value)
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
        // A format change can change this entry's registerSpan() (e.g.
        // Decimal <-> Float32/Int32) -- without retargeting, an in-flight or
        // next-cycle poll response sized for the *old* span reaches
        // FavoritesModel::applyRegisterUpdate() with a mismatched register
        // count, which used to crash on the next repaint (see
        // FavoritesModel's own defensive size-mismatch guard for the other
        // half of this fix).
        onFormatApplied: root.retargetFavoritesPollingIfActive()
    }

    FavoritesModel {
        id: favoritesModel
        addressConvention: DisplaySettings.addressConvention
        onWriteRequested: (address, value) => ConnectionController.writeSingleRegister(address, value)
        onCoilWriteRequested: (address, value) => ConnectionController.writeSingleCoil(address, value)
    }

    TagDatabaseModel {
        id: tagDatabaseModel
    }

    ImportTagFileDialog {
        id: importTagDialog
        tagDatabaseModel: tagDatabaseModel
    }

    SaveLogFileDialog {
        id: saveLogDialog
        sessionLogger: sessionLogger
        onStartFailed: statusLabel.text = "Error: could not open log file for writing"
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

    SessionLogger {
        id: sessionLogger
    }

    Connections {
        target: ConnectionController
        function onHoldingRegistersRead(startAddress, values) {
            registerModel.setRegisters(startAddress, values)
            statusLabel.text = "Read " + values.length + " register(s) starting at " + startAddress
        }
        function onBitsRead(startAddress, values) {
            registerModel.setBits(startAddress, values)
            statusLabel.text = "Read " + values.length + " bit(s) starting at " + startAddress
        }
        function onOperationFailed(message) {
            statusLabel.text = "Error: " + message
        }
        function onSingleRegisterWritten(address, value) {
            statusLabel.text = "Wrote " + value + " to register " + address
            ConnectionController.readRegisters(registerModel.registerType, root.startAddressPdu, quantityField.value)
        }
        function onSingleCoilWritten(address, value) {
            statusLabel.text = "Wrote " + (value ? "ON" : "OFF") + " to coil " + address
            ConnectionController.readRegisters(registerModel.registerType, root.startAddressPdu, quantityField.value)
        }
        function onCommunicationLogged(direction, summary) {
            communicationLogModel.append(direction, summary)
            sessionLogger.logCommunication(direction, summary)
        }
        function onRegisterReadFailed(reason) {
            registerModel.markStale()
        }
        function onConnectionLost() {
            sessionLogger.recordDisconnect()
        }
        function onConnectionRestored() {
            sessionLogger.recordReconnect()
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
                ConnectionController.startPolling(registerModel.registerType, root.startAddressPdu, quantityField.value)
        }
    }

    ThemedPopup {
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

            ThemedTextField {
                Layout.fillWidth: true
                placeholderText: "Search tags..."
                onTextChanged: tagFilterProxy.filterText = text
            }

            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: tagFilterProxy

                delegate: ThemedItemDelegate {
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
                        root.retargetFavoritesPollingIfActive()
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

        Flow {
            // A RowLayout doesn't wrap -- with this many controls, a narrower
            // window just ran them off the right edge with no way to reach
            // them (no scrollbar, nothing clipped them either). Flow wraps
            // onto a second line instead, so the toolbar always fits within
            // whatever width is available. Flow positions children directly
            // (not via Layout attached properties), so the RowLayout-only
            // fill-width spacer that used to push the rest of the toolbar
            // right is gone -- with wrapping, there's no fixed "right side"
            // to push things toward.
            Layout.fillWidth: true
            spacing: Theme.spacingMd

            Text {
                text: "ModbusViewer"
                color: Theme.accent
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeLg
                font.bold: true
            }

            Label { text: "Theme:"; color: Theme.textSecondary }
            ThemedComboBox {
                model: Theme.availableThemes
                textRole: "label"
                valueRole: "id"
                currentIndex: Math.max(0, indexOfValue(ThemeSettings.themeId))
                onActivated: ThemeSettings.themeId = currentValue
            }

            Label { text: "Address:"; color: Theme.textSecondary }
            ThemedComboBox {
                model: ["0-based (PDU)", "Modicon (4xxxx)"]
                currentIndex: DisplaySettings.addressConvention
                onActivated: (index) => DisplaySettings.addressConvention = index
            }

            Label { text: "Mode:"; color: Theme.textSecondary }
            ThemedComboBox {
                model: ["Normal", "Favorites"]
                currentIndex: PollModeController.mode
                onActivated: (index) => PollModeController.mode = index
            }

            Label { text: "Type:"; color: Theme.textSecondary }
            ThemedComboBox {
                // Values match Core::RegisterType's/RegisterTableModel::RegisterType's
                // shared ordinal ordering, so currentIndex tracks registerType directly.
                model: ["Coil (0x)", "Discrete Input (1x)", "Holding Register (4x)", "Input Register (3x)"]
                currentIndex: registerModel.registerType
                onActivated: (index) => {
                    registerModel.registerType = index
                    // Switching address space while a Normal-mode poll is running
                    // retargets immediately, same as the Normal<->Favorites mode
                    // switch above.
                    if (ConnectionController.polling && PollModeController.mode === 0)
                        ConnectionController.startPolling(registerModel.registerType, root.startAddressPdu, quantityField.value)
                }
            }

            ThemedCheckBox {
                text: "Flash on update"
                checked: DisplaySettings.flashOnUpdateEnabled
                onToggled: DisplaySettings.flashOnUpdateEnabled = checked
            }

            Label { text: "Search:"; color: Theme.textSecondary }
            ThemedTextField {
                placeholderText: "label, description, address, unit"
                width: 220
                onTextChanged: {
                    normalFilterProxy.filterText = text
                    favoritesFilterProxy.filterText = text
                }
            }

            ThemedButton {
                text: "Import Tags..."
                onClicked: importTagDialog.open()
            }

            ThemedButton {
                text: root.logPanelVisible ? "Hide Log" : "Show Log"
                onClicked: root.logPanelVisible = !root.logPanelVisible
            }

            ThemedButton {
                text: "Disconnect"
                onClicked: ConnectionController.disconnectFromDevice()
            }

            ThemedButton {
                text: sessionLogger.isLogging ? "Stop Logging" : "Start Logging..."
                onClicked: {
                    if (sessionLogger.isLogging)
                        sessionLogger.stopLogging()
                    else
                        saveLogDialog.open()
                }
            }

            Text {
                visible: sessionLogger.isLogging
                text: "Logging to " + sessionLogger.logFilePath.split(/[\\/]/).pop()
                    + " — " + sessionLogger.disconnectCount + " disconnect(s), " + sessionLogger.reconnectCount + " reconnect(s)"
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSm
            }
        }

        RowLayout {
            spacing: Theme.spacingSm

            Label { text: "Start Address"; color: Theme.textSecondary }
            ThemedSpinBox {
                id: startAddressField
                // Q_INVOKABLE calls aren't tracked as binding dependencies by QML, so
                // each expression reads addressConvention/registerType first (comma
                // operator) to force re-evaluation when either changes -- a bare
                // method call wouldn't refresh these on its own.
                from: (DisplaySettings.addressConvention, registerModel.registerType,
                       DisplaySettings.toDisplayAddress(registerModel.registerType, 0))
                to: (DisplaySettings.addressConvention, registerModel.registerType,
                     DisplaySettings.toDisplayAddress(registerModel.registerType, 65535))
                value: (DisplaySettings.addressConvention, registerModel.registerType,
                        DisplaySettings.toDisplayAddress(registerModel.registerType, root.startAddressPdu))
                onValueModified: root.startAddressPdu = DisplaySettings.toPduAddress(registerModel.registerType, value)
            }

            Label { text: "Quantity"; color: Theme.textSecondary }
            ThemedSpinBox {
                id: quantityField
                from: 1
                to: (registerModel.registerType, registerModel.maxReadCountFor())
                value: 10
            }

            ThemedButton {
                text: "Read"
                enabled: !ConnectionController.polling
                onClicked: ConnectionController.readRegisters(registerModel.registerType, root.startAddressPdu, quantityField.value)
            }

            ToolSeparator {}

            Label { text: "Interval (ms)"; color: Theme.textSecondary }
            ThemedSpinBox {
                id: intervalField
                from: 10
                to: 60000
                stepSize: 100
                value: ConnectionController.pollIntervalMs
                onValueModified: ConnectionController.pollIntervalMs = value
            }

            ThemedButton {
                text: ConnectionController.polling ? "Stop Polling" : "Start Polling"
                onClicked: {
                    if (ConnectionController.polling) {
                        ConnectionController.stopPolling()
                    } else if (PollModeController.mode === 1) {
                        ConnectionController.startPollingFavorites(favoritesModel)
                    } else {
                        ConnectionController.startPolling(registerModel.registerType, root.startAddressPdu, quantityField.value)
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
                        required property bool isBit
                        required property bool boolValue
                        required property bool writable

                        implicitWidth: registerTableView.columnWidthProvider(column)
                        implicitHeight: 40
                        // Basic-style TextField's own default implicitWidth is wider than
                        // this row's RowLayout can always afford once the column shrinks;
                        // without clipping here, an unshrunk child renders past this cell's
                        // right edge instead of inside it -- invisible until the window is
                        // widened enough to reach it, rather than just fitting or clipping.
                        clip: true
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

                            // Coil: writable, so a toggle switch that writes immediately
                            // (matches the numeric TextField's write-on-editingFinished
                            // behavior -- a toggle click is the coil equivalent of
                            // tabbing away from a text field).
                            ThemedSwitch {
                                visible: delegateRoot.isBit && delegateRoot.writable
                                checked: delegateRoot.boolValue
                                onToggled: registerModel.setBitAt(normalFilterProxy.mapRowToSource(delegateRoot.row), checked)
                            }

                            // Discrete Input: read-only status indicator, no interaction.
                            Text {
                                visible: delegateRoot.isBit && !delegateRoot.writable
                                text: delegateRoot.boolValue ? "ON" : "OFF"
                                color: delegateRoot.boolValue ? Theme.success : Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.bold: true
                            }

                            // Pushes the value field (and ⚙ beside it) to the cell's
                            // right edge now that the field itself sizes to its
                            // content rather than stretching to fill the row.
                            Item { Layout.fillWidth: true }

                            ThemedTextField {
                                id: valueField
                                visible: !delegateRoot.isBit
                                // Sized to the value's own text rather than stretched
                                // full-width -- a value like "82.4" doesn't need a
                                // field wide enough for the whole cell. Bounded so a
                                // very long formatted value (e.g. a signed Int32 with
                                // a unit suffix) still has room, and a very short one
                                // doesn't shrink to nothing.
                                Layout.preferredWidth: Math.max(60, Math.min(contentWidth + leftPadding + rightPadding + 12, 260))
                                readOnly: !delegateRoot.writable
                                // registerModel.stale is whole-range: Normal mode is
                                // always one PollTarget covering the entire visible
                                // range, so a poll failure flags every value cell
                                // (contrast Favorites' per-row staleness below).
                                color: registerModel.stale ? Theme.warning : Theme.textPrimary
                                font.family: Theme.fontFamilyMono
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

                            // Scale/offset/unit/byteOrder/format have no meaning for a
                            // 1-bit value, so the format picker is hidden entirely (not
                            // shown-disabled) for Coil/DiscreteInput rows.
                            ThemedButton {
                                text: "⚙"
                                visible: !delegateRoot.isBit
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

                        ThemedComboBox {
                            id: adhocRegisterTypeCombo
                            // Values match Core::RegisterType's ordering.
                            model: [
                                { text: "Coil", value: 0 },
                                { text: "Discrete Input", value: 1 },
                                { text: "Holding Register", value: 2 },
                                { text: "Input Register", value: 3 }
                            ]
                            textRole: "text"
                            valueRole: "value"
                        }

                        ThemedSpinBox {
                            id: adhocAddressField
                            from: 0
                            to: 65535
                        }

                        ThemedButton {
                            text: "Add Ad-hoc"
                            onClicked: {
                                favoritesModel.addAdHoc(adhocRegisterTypeCombo.currentValue, adhocAddressField.value)
                                root.retargetFavoritesPollingIfActive()
                            }
                        }

                        ThemedButton {
                            text: "Add From Tag..."
                            onClicked: addFromTagPopup.open()
                        }

                        Item { Layout.fillWidth: true }

                        Label { text: "View:"; color: Theme.textSecondary }
                        ThemedComboBox {
                            model: ["List", "Cards"]
                            currentIndex: DisplaySettings.favoritesViewMode
                            onActivated: (index) => DisplaySettings.favoritesViewMode = index
                        }
                    }

                    StackLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: DisplaySettings.favoritesViewMode

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
                            required property bool isBit
                            required property bool boolValue
                            required property bool writable

                            width: favoritesListView.width
                            height: 40
                            clip: true
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

                                // Coil: writable, so a toggle switch that writes
                                // immediately (matches the numeric TextField's
                                // write-on-editingFinished behavior).
                                ThemedSwitch {
                                    visible: favDelegateRoot.isBit && favDelegateRoot.writable
                                    checked: favDelegateRoot.boolValue
                                    onToggled: favoritesModel.setBitAt(favoritesFilterProxy.mapRowToSource(favDelegateRoot.index), checked)
                                }

                                // Discrete Input: read-only status indicator, no interaction.
                                Text {
                                    visible: favDelegateRoot.isBit && !favDelegateRoot.writable
                                    text: favDelegateRoot.boolValue ? "ON" : "OFF"
                                    color: favDelegateRoot.boolValue ? Theme.success : Theme.textSecondary
                                    font.family: Theme.fontFamily
                                    font.bold: true
                                }

                                // Keeps the value field/switch/pill and the ⚙/✕
                                // buttons clustered at the row's right edge, now that
                                // the value field itself sizes to its content instead
                                // of stretching to fill the row.
                                Item { Layout.fillWidth: true }

                                ThemedTextField {
                                    id: favValueField
                                    visible: !favDelegateRoot.isBit
                                    Layout.preferredWidth: Math.max(60, Math.min(contentWidth + leftPadding + rightPadding + 12, 260))
                                    readOnly: !favDelegateRoot.writable
                                    // Per-row, unlike Normal's whole-range staleness: each
                                    // Favorites entry is its own independent PollTarget.
                                    color: favDelegateRoot.stale ? Theme.warning : Theme.textPrimary
                                    font.family: Theme.fontFamilyMono
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

                                // Scale/offset/unit/byteOrder/format have no meaning for a
                                // 1-bit value, so the format picker is hidden entirely for
                                // Coil/DiscreteInput rows.
                                ThemedButton {
                                    text: "⚙"
                                    visible: !favDelegateRoot.isBit
                                    flat: true
                                    Layout.preferredWidth: 32
                                    onClicked: favoritesFormatPicker.openFor(favoritesFilterProxy.mapRowToSource(favDelegateRoot.index))
                                }

                                ThemedButton {
                                    text: "✕"
                                    flat: true
                                    Layout.preferredWidth: 32
                                    onClicked: {
                                        favoritesModel.removeAt(favoritesFilterProxy.mapRowToSource(favDelegateRoot.index))
                                        root.retargetFavoritesPollingIfActive()
                                    }
                                }
                            }
                        }
                    }

                    GridView {
                        id: favoritesGridView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: favoritesFilterProxy
                        cellWidth: 200
                        cellHeight: 120

                        delegate: Rectangle {
                            id: cardDelegateRoot
                            required property int index
                            required property string label
                            required property string address
                            required property string value
                            required property bool stale
                            required property bool isBit
                            required property bool boolValue
                            required property bool writable
                            required property var history

                            width: favoritesGridView.cellWidth - Theme.spacingSm
                            height: favoritesGridView.cellHeight - Theme.spacingSm
                            color: Theme.surface
                            radius: Theme.radiusMd
                            border.color: Theme.border
                            onValueChanged: if (DisplaySettings.flashOnUpdateEnabled) cardFlashAnimation.restart()

                            // A brief background tint on update, declared first so it
                            // renders behind the card's own content rather than covering it.
                            Rectangle {
                                anchors.fill: parent
                                radius: parent.radius
                                color: Theme.accent
                                opacity: 0
                                NumberAnimation on opacity {
                                    id: cardFlashAnimation
                                    running: false
                                    from: 0.35
                                    to: 0
                                    duration: 400
                                }
                            }

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: Theme.spacingSm
                                spacing: Theme.spacingSm

                                RowLayout {
                                    spacing: Theme.spacingXs

                                    Text {
                                        text: cardDelegateRoot.label + " (" + cardDelegateRoot.address + ")"
                                        color: Theme.textSecondary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeSm
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    // Scale/offset/unit/byteOrder/format have no meaning
                                    // for a 1-bit value, so hidden entirely for bit rows.
                                    ThemedButton {
                                        text: "⚙"
                                        visible: !cardDelegateRoot.isBit
                                        flat: true
                                        Layout.preferredWidth: 24
                                        onClicked: favoritesFormatPicker.openFor(favoritesFilterProxy.mapRowToSource(cardDelegateRoot.index))
                                    }

                                    ThemedButton {
                                        text: "✕"
                                        flat: true
                                        Layout.preferredWidth: 24
                                        onClicked: {
                                            favoritesModel.removeAt(favoritesFilterProxy.mapRowToSource(cardDelegateRoot.index))
                                            root.retargetFavoritesPollingIfActive()
                                        }
                                    }
                                }

                                // Coil: writable toggle, matches the list view's behavior.
                                ThemedSwitch {
                                    visible: cardDelegateRoot.isBit && cardDelegateRoot.writable
                                    checked: cardDelegateRoot.boolValue
                                    onToggled: favoritesModel.setBitAt(favoritesFilterProxy.mapRowToSource(cardDelegateRoot.index), checked)
                                }

                                // Discrete Input: read-only status indicator.
                                Text {
                                    visible: cardDelegateRoot.isBit && !cardDelegateRoot.writable
                                    text: cardDelegateRoot.boolValue ? "ON" : "OFF"
                                    color: cardDelegateRoot.boolValue ? Theme.success : Theme.textSecondary
                                    font.bold: true
                                    font.pixelSize: Theme.fontSizeLg
                                }

                                // Word types: big number + a sparkline of recent history.
                                ColumnLayout {
                                    visible: !cardDelegateRoot.isBit
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    spacing: Theme.spacingXs

                                    Text {
                                        text: cardDelegateRoot.value
                                        color: cardDelegateRoot.stale ? Theme.warning : Theme.textPrimary
                                        font.family: Theme.fontFamilyMono
                                        font.bold: true
                                        font.pixelSize: Theme.fontSizeLg
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    // No severity/threshold system exists yet, so the
                                    // sparkline is plain accent-colored chart ink, not
                                    // status-coded -- Theme.accent is already this app's
                                    // "live/changed data" color (the row-update flash).
                                    Canvas {
                                        id: sparkline
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true

                                        onPaint: {
                                            const ctx = getContext("2d")
                                            ctx.reset()
                                            const pts = cardDelegateRoot.history
                                            if (pts.length < 2)
                                                return // not enough history yet -- leave blank

                                            let min = pts[0]
                                            let max = pts[0]
                                            for (const p of pts) {
                                                if (p < min) min = p
                                                if (p > max) max = p
                                            }
                                            const range = (max - min) || 1 // avoid div-by-zero when flat

                                            ctx.strokeStyle = Theme.accent
                                            ctx.lineWidth = 2
                                            ctx.beginPath()
                                            for (let i = 0; i < pts.length; ++i) {
                                                const x = (i / (pts.length - 1)) * width
                                                const y = height - ((pts[i] - min) / range) * height
                                                if (i === 0)
                                                    ctx.moveTo(x, y)
                                                else
                                                    ctx.lineTo(x, y)
                                            }
                                            ctx.stroke()
                                        }

                                        Connections {
                                            target: cardDelegateRoot
                                            function onHistoryChanged() { sparkline.requestPaint() }
                                        }
                                    }
                                }
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

                    ThemedButton {
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
