#pragma once

#include <QtGlobal>

#include "model/RegisterType.h"

namespace ModbusViewer::Core {

// One contiguous span of registers the app wants kept live. A Normal-mode view is a
// single target; Favorites is many scattered ones (see ReadCoalescer).
struct PollTarget
{
    quint8 unitId = 1;
    RegisterType registerType = RegisterType::HoldingRegister;
    quint16 startAddress = 0;
    quint16 quantity = 1;

    int endAddressExclusive() const { return int(startAddress) + int(quantity); }
};

} // namespace ModbusViewer::Core
