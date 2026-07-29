#include "ModbusException.h"

namespace ModbusViewer::Core {

QString describeModbusExceptionCode(ModbusExceptionCode code)
{
    switch (code) {
    case ModbusExceptionCode::IllegalFunction:
        return QStringLiteral("Illegal Function");
    case ModbusExceptionCode::IllegalDataAddress:
        return QStringLiteral("Illegal Data Address");
    case ModbusExceptionCode::IllegalDataValue:
        return QStringLiteral("Illegal Data Value");
    case ModbusExceptionCode::SlaveDeviceFailure:
        return QStringLiteral("Slave Device Failure");
    case ModbusExceptionCode::Acknowledge:
        return QStringLiteral("Acknowledge");
    case ModbusExceptionCode::SlaveDeviceBusy:
        return QStringLiteral("Slave Device Busy");
    case ModbusExceptionCode::MemoryParityError:
        return QStringLiteral("Memory Parity Error");
    case ModbusExceptionCode::GatewayPathUnavailable:
        return QStringLiteral("Gateway Path Unavailable");
    case ModbusExceptionCode::GatewayTargetDeviceFailedToRespond:
        return QStringLiteral("Gateway Target Device Failed to Respond");
    }
    return QStringLiteral("Unknown Exception (0x%1)").arg(quint8(code), 2, 16, QChar('0'));
}

bool isExceptionResponseFunctionCode(quint8 functionCode)
{
    return (functionCode & 0x80) != 0;
}

} // namespace ModbusViewer::Core
