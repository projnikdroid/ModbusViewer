#include "RegisterType.h"

namespace ModbusViewer::Core {

FunctionCode readFunctionCodeFor(RegisterType type)
{
    switch (type) {
    case RegisterType::Coil:
        return FunctionCode::ReadCoils;
    case RegisterType::DiscreteInput:
        return FunctionCode::ReadDiscreteInputs;
    case RegisterType::HoldingRegister:
        return FunctionCode::ReadHoldingRegisters;
    case RegisterType::InputRegister:
        return FunctionCode::ReadInputRegisters;
    }
    return FunctionCode::ReadHoldingRegisters;
}

bool isBitRegisterType(RegisterType type)
{
    return type == RegisterType::Coil || type == RegisterType::DiscreteInput;
}

bool isWritableRegisterType(RegisterType type)
{
    return type == RegisterType::Coil || type == RegisterType::HoldingRegister;
}

int maxReadCountFor(RegisterType type)
{
    return isBitRegisterType(type) ? MaxBitsPerReadRequest : MaxRegistersPerReadRequest;
}

} // namespace ModbusViewer::Core
