#include "ModbusRtuFramer.h"

#include "Crc16.h"
#include "ModbusException.h"
#include "ModbusPduCodec.h"

namespace ModbusViewer::Core {

namespace {
constexpr int CrcByteCount = 2;
constexpr int MinimumFrameByteCount = 1 + CrcByteCount; // unit id + crc, empty pdu

constexpr int UnitIdAndFunctionCodeByteCount = 2;
constexpr int ExceptionFrameByteCount = 5;    // unit + fc + exception code + crc
constexpr int WriteResponseByteCount = 8;     // unit + fc + address + value/quantity + crc
constexpr int ReadResponseOverheadBytes = 5;  // unit + fc + byteCount + crc

bool isReadFunctionCode(quint8 functionCode)
{
    switch (FunctionCode(functionCode)) {
    case FunctionCode::ReadCoils:
    case FunctionCode::ReadDiscreteInputs:
    case FunctionCode::ReadHoldingRegisters:
    case FunctionCode::ReadInputRegisters:
        return true;
    default:
        return false;
    }
}

bool isWriteFunctionCode(quint8 functionCode)
{
    switch (FunctionCode(functionCode)) {
    case FunctionCode::WriteSingleCoil:
    case FunctionCode::WriteSingleRegister:
    case FunctionCode::WriteMultipleCoils:
    case FunctionCode::WriteMultipleRegisters:
        return true;
    default:
        return false;
    }
}

} // namespace

QByteArray encodeRtuFrame(quint8 unitId, const QByteArray &pdu)
{
    QByteArray frame;
    frame.reserve(1 + pdu.size() + CrcByteCount);
    frame.append(char(unitId));
    frame.append(pdu);

    const quint16 crc = computeModbusCrc16(frame);
    frame.append(char(crc & 0xFF));
    frame.append(char((crc >> 8) & 0xFF));

    return frame;
}

std::optional<DecodedRtuFrame> decodeRtuFrame(const QByteArray &frame)
{
    if (frame.size() < MinimumFrameByteCount)
        return std::nullopt;

    const QByteArray withoutCrc = frame.left(frame.size() - CrcByteCount);
    const quint16 receivedCrc =
        quint16(quint8(frame[frame.size() - 2])) | (quint16(quint8(frame[frame.size() - 1])) << 8);

    if (computeModbusCrc16(withoutCrc) != receivedCrc)
        return std::nullopt;

    DecodedRtuFrame decoded;
    decoded.unitId = quint8(withoutCrc[0]);
    decoded.pdu = withoutCrc.mid(1);
    return decoded;
}

int expectedRtuResponseLength(const QByteArray &partialFrame)
{
    if (partialFrame.size() < UnitIdAndFunctionCodeByteCount)
        return 0;

    const quint8 functionCode = quint8(partialFrame[1]);

    if (isExceptionResponseFunctionCode(functionCode))
        return ExceptionFrameByteCount;

    if (isWriteFunctionCode(functionCode))
        return WriteResponseByteCount;

    if (isReadFunctionCode(functionCode)) {
        if (partialFrame.size() < 3)
            return 0; // byte count field not received yet
        return ReadResponseOverheadBytes + quint8(partialFrame[2]);
    }

    return 0; // unrecognized function code - let the read time out rather than guess
}

} // namespace ModbusViewer::Core
