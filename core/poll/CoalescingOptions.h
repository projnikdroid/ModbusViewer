#pragma once

#include "model/RegisterType.h"

namespace ModbusViewer::Core {

struct CoalescingOptions
{
    // How many unwanted registers are worth pulling in to avoid a second round trip.
    // One extra request costs a full request/response latency, so bridging a small
    // dead span is usually cheaper than reading around it. 0 merges only adjacent or
    // overlapping targets.
    int maxGapToBridge = 8;

    // Per-request ceilings. Default to the Modbus spec limits; overridable because
    // some gateways and devices advertise smaller ones.
    int maxBitsPerRequest = MaxBitsPerReadRequest;
    int maxRegistersPerRequest = MaxRegistersPerReadRequest;

    int maxCountFor(RegisterType type) const
    {
        return isBitRegisterType(type) ? maxBitsPerRequest : maxRegistersPerRequest;
    }
};

} // namespace ModbusViewer::Core
