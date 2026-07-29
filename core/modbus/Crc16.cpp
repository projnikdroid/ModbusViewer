#include "Crc16.h"

namespace ModbusViewer::Core {

quint16 computeModbusCrc16(const QByteArray &data)
{
    quint16 crc = 0xFFFF;

    for (unsigned char byte : data) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            const bool lsbSet = (crc & 0x0001) != 0;
            crc >>= 1;
            if (lsbSet)
                crc ^= 0xA001;
        }
    }

    return crc;
}

} // namespace ModbusViewer::Core
