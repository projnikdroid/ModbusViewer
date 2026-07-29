#pragma once

#include "model/RegisterType.h"

namespace ModbusViewer::Core {

// A pure display-layer transform: the wire/PDU address is always 0-based and never
// changes. Modicon is the common 5-digit-per-region datasheet convention (0xxxx
// coils, 1xxxx discrete inputs, 3xxxx input registers, 4xxxx holding registers),
// where each region's "1" is PDU address 0.
enum class AddressConvention { Pdu, Modicon };

int displayAddress(RegisterType type, int pduAddress, AddressConvention convention);

int pduAddress(RegisterType type, int displayAddress, AddressConvention convention);

} // namespace ModbusViewer::Core
