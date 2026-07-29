#pragma once

#include <QString>
#include <QtGlobal>

namespace ModbusViewer::Core {

enum class ModbusExceptionCode : quint8 {
    IllegalFunction = 0x01,
    IllegalDataAddress = 0x02,
    IllegalDataValue = 0x03,
    SlaveDeviceFailure = 0x04,
    Acknowledge = 0x05,
    SlaveDeviceBusy = 0x06,
    MemoryParityError = 0x08,
    GatewayPathUnavailable = 0x0A,
    GatewayTargetDeviceFailedToRespond = 0x0B,
};

// Human-readable description of a Modbus exception code, for the communication log
// and register error state. Unrecognized codes still get a useful message rather
// than an empty string.
QString describeModbusExceptionCode(ModbusExceptionCode code);

struct ModbusExceptionResponse
{
    quint8 originalFunctionCode = 0;
    ModbusExceptionCode exceptionCode = ModbusExceptionCode::IllegalFunction;
    QString message;
};

// A response PDU's function code has the high bit set (0x80) when the device
// returned an exception instead of the requested data.
bool isExceptionResponseFunctionCode(quint8 functionCode);

} // namespace ModbusViewer::Core
