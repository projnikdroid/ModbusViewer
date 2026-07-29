#pragma once

#include "modbus/ModbusPduCodec.h"

namespace ModbusViewer::Core {

// The four Modbus data regions. Distinct from FunctionCode: a region says *what* is
// being addressed, a function code says what operation is being performed on it.
// Coils and discrete inputs are single bits; holding and input registers are 16-bit.
enum class RegisterType {
    Coil,
    DiscreteInput,
    HoldingRegister,
    InputRegister,
};

FunctionCode readFunctionCodeFor(RegisterType type);

bool isBitRegisterType(RegisterType type);

// Per-request ceilings from the Modbus spec: 2000 bits or 125 registers.
constexpr int MaxBitsPerReadRequest = 2000;
constexpr int MaxRegistersPerReadRequest = 125;

int maxReadCountFor(RegisterType type);

} // namespace ModbusViewer::Core
