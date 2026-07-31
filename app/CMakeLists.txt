qt_policy(SET QTP0004 NEW)

qt_add_executable(ModbusViewer
    main.cpp
    $<$<PLATFORM_ID:Windows>:app.rc>
)

# app.rc embeds icons/app.ico as the .exe's own resource icon (Explorer,
# taskbar default, Alt-Tab before the window paints). The same file is also
# embedded as a Qt resource below so main.cpp can set it as the *runtime*
# window/taskbar icon via QGuiApplication::setWindowIcon() -- Qt doesn't pick
# the .exe's resource icon up automatically for that.
qt_add_resources(ModbusViewer "appicon"
    PREFIX "/"
    FILES icons/app.ico
)

set_source_files_properties(qml/theme/Theme.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(ModbusViewer
    URI ModbusViewer
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/theme/Theme.qml
        qml/screens/ConnectionScreen.qml
        qml/screens/MainScreen.qml
        qml/connection/TcpSettingsPanel.qml
        qml/connection/RtuSettingsPanel.qml
        qml/components/DisconnectedWatermark.qml
        qml/components/HandshakeAnimation.qml
        qml/components/FormatPicker.qml
        qml/components/ThemeBackdrop.qml
        qml/controls/ThemedButton.qml
        qml/controls/ThemedComboBox.qml
        qml/controls/ThemedSpinBox.qml
        qml/controls/ThemedTextField.qml
        qml/controls/ThemedSwitch.qml
        qml/controls/ThemedCheckBox.qml
        qml/controls/ThemedTabBar.qml
        qml/controls/ThemedTabButton.qml
        qml/controls/ThemedPopup.qml
        qml/controls/ThemedItemDelegate.qml
        qml/dialogs/ImportTagFileDialog.qml
    SOURCES
        ${MODBUSVIEWER_APPLIB_SOURCES}
)

target_include_directories(ModbusViewer PRIVATE ${MODBUSVIEWER_APPLIB_INCLUDE_DIRS})

target_link_libraries(ModbusViewer PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickDialogs2
    Qt6::Network
    Qt6::SerialPort
    modbusviewer_core
)

# Release is a windowed app; Debug keeps a console so QML runtime errors and
# qDebug output are actually visible. A QML TypeError in a windowed build fails
# completely silently, which is expensive to diagnose.
set_target_properties(ModbusViewer PROPERTIES
    WIN32_EXECUTABLE $<NOT:$<CONFIG:Debug>>
    MACOSX_BUNDLE TRUE
)
