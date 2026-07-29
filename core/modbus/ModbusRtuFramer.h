#pragma once

#include <optional>

#include <QByteArray>
#include <QtGlobal>

namespace ModbusViewer::Core {

struct DecodedRtuFrame
{
    quint8 unitId = 0;
    QByteArray pdu;
};

// Unit ID + PDU + CRC16 (low byte first, then high byte).
QByteArray encodeRtuFrame(quint8 unitId, const QByteArray &pdu);

// Returns nullopt if the frame is too short to contain a valid CRC, or the CRC
// does not match (corrupted frame) - both are properties of the bytes themselves,
// not something a caller needs to distinguish.
std::optional<DecodedRtuFrame> decodeRtuFrame(const QByteArray &frame);

// RTU frames carry no length field (unlike Modbus TCP's MBAP header), so a reader
// assembling a serial byte stream has to infer the total frame size from the
// function code and, for reads, the byte-count field. Returns 0 while more bytes
// are still needed to make that determination.
int expectedRtuResponseLength(const QByteArray &partialFrame);

} // namespace ModbusViewer::Core
