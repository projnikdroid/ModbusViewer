#pragma once

#include <QByteArray>
#include <QtGlobal>

namespace ModbusViewer::Core {

// Modbus RTU CRC16 (poly 0xA001, init 0xFFFF, little-endian result).
quint16 computeModbusCrc16(const QByteArray &data);

} // namespace ModbusViewer::Core
