cmake_minimum_required(VERSION 3.21)
project(ModbusViewer VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 QuickDialogs2 Network SerialPort Test)
qt_standard_project_setup(REQUIRES 6.5)

enable_testing()

# app-lib/ holds the QObject/QAbstractItemModel classes that bridge core/ to QML.
# They are compiled into the app target (qt_add_qml_module needs them there for
# QML_ELEMENT registration) and into whichever test targets exercise them, rather
# than living in their own library: a static library carrying the QML module would
# force the .qml files to move under app-lib/ too, and a library without the module
# would have both it and the app running moc over the same headers, producing
# duplicate QMetaObject symbols. Recompiling a handful of files is the cheaper
# trade. Revisit if app-lib/ grows substantially.
set(MODBUSVIEWER_APPLIB_SOURCES
    ${CMAKE_SOURCE_DIR}/app-lib/CommunicationLogModel.h
    ${CMAKE_SOURCE_DIR}/app-lib/CommunicationLogModel.cpp
    ${CMAKE_SOURCE_DIR}/app-lib/ConnectionController.h
    ${CMAKE_SOURCE_DIR}/app-lib/ConnectionController.cpp
    ${CMAKE_SOURCE_DIR}/app-lib/DisplaySettings.h
    ${CMAKE_SOURCE_DIR}/app-lib/DisplaySettings.cpp
    ${CMAKE_SOURCE_DIR}/app-lib/PollModeController.h
    ${CMAKE_SOURCE_DIR}/app-lib/PollModeController.cpp
    ${CMAKE_SOURCE_DIR}/app-lib/TagDatabaseController.h
    ${CMAKE_SOURCE_DIR}/app-lib/TagDatabaseController.cpp
    ${CMAKE_SOURCE_DIR}/app-lib/ThemeSettings.h
    ${CMAKE_SOURCE_DIR}/app-lib/ThemeSettings.cpp
    ${CMAKE_SOURCE_DIR}/app-lib/models/RegisterTableModel.h
    ${CMAKE_SOURCE_DIR}/app-lib/models/RegisterTableModel.cpp
    ${CMAKE_SOURCE_DIR}/app-lib/models/TagDatabaseModel.h
    ${CMAKE_SOURCE_DIR}/app-lib/models/TagDatabaseModel.cpp
    ${CMAKE_SOURCE_DIR}/app-lib/models/FavoritesModel.h
    ${CMAKE_SOURCE_DIR}/app-lib/models/FavoritesModel.cpp
    ${CMAKE_SOURCE_DIR}/app-lib/models/RegisterFilterProxyModel.h
    ${CMAKE_SOURCE_DIR}/app-lib/models/RegisterFilterProxyModel.cpp
    ${CMAKE_SOURCE_DIR}/app-lib/serial/SerialPortListModel.h
    ${CMAKE_SOURCE_DIR}/app-lib/serial/SerialPortListModel.cpp
)

set(MODBUSVIEWER_APPLIB_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/app-lib
    ${CMAKE_SOURCE_DIR}/app-lib/models
    ${CMAKE_SOURCE_DIR}/app-lib/serial
)

add_subdirectory(core)
add_subdirectory(app)
add_subdirectory(tests)
