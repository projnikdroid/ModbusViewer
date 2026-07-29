add_library(modbusviewer_core STATIC
    modbus/Crc16.cpp
    modbus/ModbusException.cpp
    modbus/ModbusRtuFramer.cpp
    modbus/ModbusTcpFramer.cpp
    modbus/ModbusPduCodec.cpp
    modbus/ModbusTransactionManager.cpp
    modbus/RtuTiming.cpp
    model/RegisterType.cpp
    format/ValueFormatter.cpp
    format/AddressConvention.cpp
    importer/TagRowParser.cpp
    importer/CsvTagParser.cpp
    importer/JsonTagParser.cpp
    poll/PollEngine.cpp
    poll/ReadCoalescer.cpp
    transport/ITransport.h
    transport/TcpTransport.cpp
    transport/SerialTransport.cpp
)

target_include_directories(modbusviewer_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(modbusviewer_core PUBLIC Qt6::Core Qt6::Network Qt6::SerialPort)
