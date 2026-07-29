#include "AddressConvention.h"

namespace ModbusViewer::Core {

namespace {

int modiconBaseFor(RegisterType type)
{
    switch (type) {
    case RegisterType::Coil:
        return 1;
    case RegisterType::DiscreteInput:
        return 10001;
    case RegisterType::InputRegister:
        return 30001;
    case RegisterType::HoldingRegister:
        return 40001;
    }
    return 1;
}

} // namespace

int displayAddress(RegisterType type, int pduAddress, AddressConvention convention)
{
    if (convention == AddressConvention::Pdu)
        return pduAddress;
    return modiconBaseFor(type) + pduAddress;
}

int pduAddress(RegisterType type, int displayAddress, AddressConvention convention)
{
    if (convention == AddressConvention::Pdu)
        return displayAddress;
    return displayAddress - modiconBaseFor(type);
}

} // namespace ModbusViewer::Core
