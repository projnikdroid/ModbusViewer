#include "ModbusTcpFramer.h"

namespace ModbusViewer::Core {

namespace {

void appendBigEndianU16(QByteArray &buffer, quint16 value)
{
    buffer.append(char((value >> 8) & 0xFF));
    buffer.append(char(value & 0xFF));
}

quint16 readBigEndianU16(const QByteArray &buffer, int offset)
{
    return (quint16(quint8(buffer[offset])) << 8) | quint16(quint8(buffer[offset + 1]));
}

} // namespace

QByteArray encodeMbapFrame(quint16 transactionId, quint8 unitId, const QByteArray &pdu)
{
    QByteArray frame;
    frame.reserve(MbapHeaderByteCount + pdu.size());

    appendBigEndianU16(frame, transactionId);
    appendBigEndianU16(frame, 0); // protocol id, always 0 for Modbus TCP
    appendBigEndianU16(frame, quint16(1 + pdu.size())); // unit id + pdu
    frame.append(char(unitId));
    frame.append(pdu);

    return frame;
}

std::optional<DecodedMbapFrame> decodeMbapFrame(const QByteArray &buffer)
{
    if (buffer.size() < MbapHeaderByteCount)
        return std::nullopt;

    const quint16 transactionId = readBigEndianU16(buffer, 0);
    const quint16 protocolId = readBigEndianU16(buffer, 2);
    const quint16 length = readBigEndianU16(buffer, 4);
    const quint8 unitId = quint8(buffer[6]);

    if (protocolId != 0)
        return std::nullopt;

    const int totalFrameSize = 6 + length; // transaction id + protocol id + length field don't count length itself
    if (buffer.size() < totalFrameSize)
        return std::nullopt; // caller should keep buffering

    DecodedMbapFrame decoded;
    decoded.transactionId = transactionId;
    decoded.unitId = unitId;
    decoded.pdu = buffer.mid(MbapHeaderByteCount, length - 1);
    decoded.frameByteCount = totalFrameSize;
    return decoded;
}

} // namespace ModbusViewer::Core
