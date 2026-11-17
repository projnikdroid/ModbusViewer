# QML behavior tests for the app's themed input controls and the RTU settings panel.
#
# These run the app's REAL QML files, not copies of their logic: at configure time
# the real ThemedSpinBox/ThemedComboBox/ThemedButton/ThemedItemDelegate and
# connection/RtuSettingsPanel.qml are copied into a throwaway "ModbusViewer" QML
# module next to stub versions of the three things they depend on that live outside
# QML (Theme, ConnectionController, SerialPortListModel -- see stubs/). configure_file
# tracks its inputs, so editing a real file re-copies it on the next build.
set(QML_IMPORT_DIR ${CMAKE_CURRENT_BINARY_DIR}/qml_import)
set(QML_MODULE_DIR ${QML_IMPORT_DIR}/ModbusViewer)

set(QML_TEST_REAL_FILES
    controls/ThemedSpinBox
    controls/ThemedComboBox
    controls/ThemedItemDelegate
    controls/ThemedButton
    connection/RtuSettingsPanel
)

set(qmldir "module ModbusViewer\n")
string(APPEND qmldir "singleton Theme 1.0 Theme.qml\n")
string(APPEND qmldir "singleton ConnectionController 1.0 ConnectionController.qml\n")
string(APPEND qmldir "SerialPortListModel 1.0 SerialPortListModel.qml\n")
foreach(stub Theme ConnectionController SerialPortListModel)
    configure_file(stubs/${stub}.qml ${QML_MODULE_DIR}/${stub}.qml COPYONLY)
endforeach()
foreach(real ${QML_TEST_REAL_FILES})
    get_filename_component(name ${real} NAME)
    configure_file(${CMAKE_SOURCE_DIR}/app/qml/${real}.qml ${QML_MODULE_DIR}/${name}.qml COPYONLY)
    string(APPEND qmldir "${name} 1.0 ${name}.qml\n")
endforeach()
file(WRITE ${QML_MODULE_DIR}/qmldir "${qmldir}")

add_executable(test_qml_controls main.cpp)
target_link_libraries(test_qml_controls PRIVATE Qt6::QuickTest Qt6::Quick)
target_compile_definitions(test_qml_controls PRIVATE QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME test_qml_controls COMMAND test_qml_controls -import ${QML_IMPORT_DIR})
