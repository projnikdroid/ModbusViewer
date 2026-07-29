#pragma once

#include <optional>

#include <QByteArray>
#include <QtGlobal>

namespace ModbusViewer::Core {

struct DecodedMbapFrame
{
    quint16 transactionId = 0;
    quint8 unitId = 0;
    QByteArray pdu;
    // Total bytes this frame occupied in the buffer it was decoded from - callers
    // reassembling a TCP byte stream use this to trim off exactly one frame and
    // keep buffering the remainder.
    int frameByteCount = 0;
};

// MBAP header (transaction id, protocol id = 0, length, unit id) followed by the PDU.
QByteArray encodeMbapFrame(quint16 transactionId, quint8 unitId, const QByteArray &pdu);

// Returns nullopt if the buffer doesn't yet contain a complete frame (the caller
// should keep buffering more bytes from the socket) or the protocol id isn't 0
// (not a Modbus TCP frame).
std::optional<DecodedMbapFrame> decodeMbapFrame(const QByteArray &buffer);

// How many bytes decodeMbapFrame needs before it can even read the length field.
constexpr int MbapHeaderByteCount = 7;

} // namespace ModbusViewer::Core
