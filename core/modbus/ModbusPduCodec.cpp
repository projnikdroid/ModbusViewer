#include "ModbusPduCodec.h"

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

QByteArray packBits(const QList<bool> &values)
{
    QByteArray packed((values.size() + 7) / 8, char(0));
    for (int i = 0; i < values.size(); ++i) {
        if (values[i])
            packed[i / 8] = char(quint8(packed[i / 8]) | (1 << (i % 8)));
    }
    return packed;
}

struct ResponseHeaderCheck
{
    bool isException = false;
    bool isValid = false;
    QString errorMessage;
    ModbusExceptionResponse exception;
};

// Shared by every decode*Response function: detects an exception response, or
// confirms the response's function code matches what was requested. Templated
// decode functions can't share a single return type here, so each caller
// branches on isException/isValid and wraps the result in its own
// PduDecodeResult<T>.
ResponseHeaderCheck checkResponseHeader(const QByteArray &pdu, quint8 expectedFunctionCode)
{
    ResponseHeaderCheck check;

    if (pdu.isEmpty()) {
        check.errorMessage = QStringLiteral("empty PDU");
        return check;
    }

    const quint8 responseFunctionCode = quint8(pdu[0]);

    if (isExceptionResponseFunctionCode(responseFunctionCode)) {
        if (pdu.size() < 2) {
            check.errorMessage = QStringLiteral("exception response missing exception code byte");
            return check;
        }
        ModbusExceptionResponse exception;
        exception.originalFunctionCode = responseFunctionCode & 0x7F;
        exception.exceptionCode = ModbusExceptionCode(quint8(pdu[1]));
        exception.message = describeModbusExceptionCode(exception.exceptionCode);
        check.isException = true;
        check.exception = exception;
        return check;
    }

    if (responseFunctionCode != expectedFunctionCode) {
        check.errorMessage = QStringLiteral("unexpected function code 0x%1, expected 0x%2")
                                  .arg(responseFunctionCode, 2, 16, QChar('0'))
                                  .arg(expectedFunctionCode, 2, 16, QChar('0'));
        return check;
    }

    check.isValid = true;
    return check;
}

PduDecodeResult<ReadRegistersResponse> decodeReadRegistersResponseImpl(const QByteArray &pdu,
                                                                         quint8 expectedFunctionCode)
{
    const auto header = checkResponseHeader(pdu, expectedFunctionCode);
    if (header.isException)
        return PduDecodeResult<ReadRegistersResponse>::exceptionResponse(header.exception);
    if (!header.isValid)
        return PduDecodeResult<ReadRegistersResponse>::malformed(header.errorMessage);

    if (pdu.size() < 2)
        return PduDecodeResult<ReadRegistersResponse>::malformed(QStringLiteral("missing byte count"));

    const quint8 byteCount = quint8(pdu[1]);
    if (byteCount % 2 != 0)
        return PduDecodeResult<ReadRegistersResponse>::malformed(
            QStringLiteral("odd byte count for register response"));
    if (pdu.size() < 2 + byteCount)
        return PduDecodeResult<ReadRegistersResponse>::malformed(
            QStringLiteral("pdu shorter than declared byte count"));

    ReadRegistersResponse response;
    for (int i = 0; i < byteCount; i += 2)
        response.values.append(readBigEndianU16(pdu, 2 + i));

    return PduDecodeResult<ReadRegistersResponse>::success(std::move(response));
}

PduDecodeResult<ReadBitsResponse> decodeReadBitsResponseImpl(const QByteArray &pdu,
                                                               quint8 expectedFunctionCode,
                                                               quint16 requestedQuantity)
{
    const auto header = checkResponseHeader(pdu, expectedFunctionCode);
    if (header.isException)
        return PduDecodeResult<ReadBitsResponse>::exceptionResponse(header.exception);
    if (!header.isValid)
        return PduDecodeResult<ReadBitsResponse>::malformed(header.errorMessage);

    if (pdu.size() < 2)
        return PduDecodeResult<ReadBitsResponse>::malformed(QStringLiteral("missing byte count"));

    const quint8 byteCount = quint8(pdu[1]);
    if (pdu.size() < 2 + byteCount)
        return PduDecodeResult<ReadBitsResponse>::malformed(
            QStringLiteral("pdu shorter than declared byte count"));

    const int expectedByteCount = (requestedQuantity + 7) / 8;
    if (byteCount != expectedByteCount)
        return PduDecodeResult<ReadBitsResponse>::malformed(
            QStringLiteral("byte count does not match requested quantity"));

    ReadBitsResponse response;
    for (int bit = 0; bit < requestedQuantity; ++bit) {
        const int byteIndex = 2 + bit / 8;
        const int bitIndex = bit % 8;
        response.values.append((quint8(pdu[byteIndex]) & (1 << bitIndex)) != 0);
    }

    return PduDecodeResult<ReadBitsResponse>::success(std::move(response));
}

PduDecodeResult<WriteSingleResponse> decodeWriteSingleResponseImpl(const QByteArray &pdu,
                                                                     quint8 expectedFunctionCode)
{
    const auto header = checkResponseHeader(pdu, expectedFunctionCode);
    if (header.isException)
        return PduDecodeResult<WriteSingleResponse>::exceptionResponse(header.exception);
    if (!header.isValid)
        return PduDecodeResult<WriteSingleResponse>::malformed(header.errorMessage);

    if (pdu.size() < 5)
        return PduDecodeResult<WriteSingleResponse>::malformed(
            QStringLiteral("write response shorter than 5 bytes"));

    WriteSingleResponse response;
    response.address = readBigEndianU16(pdu, 1);
    response.value = readBigEndianU16(pdu, 3);
    return PduDecodeResult<WriteSingleResponse>::success(response);
}

PduDecodeResult<WriteMultipleResponse> decodeWriteMultipleResponseImpl(const QByteArray &pdu,
                                                                         quint8 expectedFunctionCode)
{
    const auto header = checkResponseHeader(pdu, expectedFunctionCode);
    if (header.isException)
        return PduDecodeResult<WriteMultipleResponse>::exceptionResponse(header.exception);
    if (!header.isValid)
        return PduDecodeResult<WriteMultipleResponse>::malformed(header.errorMessage);

    if (pdu.size() < 5)
        return PduDecodeResult<WriteMultipleResponse>::malformed(
            QStringLiteral("write response shorter than 5 bytes"));

    WriteMultipleResponse response;
    response.startAddress = readBigEndianU16(pdu, 1);
    response.quantity = readBigEndianU16(pdu, 3);
    return PduDecodeResult<WriteMultipleResponse>::success(response);
}

} // namespace

QByteArray encodeReadRequest(FunctionCode functionCode, quint16 startAddress, quint16 quantity)
{
    QByteArray pdu;
    pdu.append(char(quint8(functionCode)));
    appendBigEndianU16(pdu, startAddress);
    appendBigEndianU16(pdu, quantity);
    return pdu;
}

QByteArray encodeWriteSingleCoilRequest(quint16 address, bool value)
{
    QByteArray pdu;
    pdu.append(char(quint8(FunctionCode::WriteSingleCoil)));
    appendBigEndianU16(pdu, address);
    appendBigEndianU16(pdu, value ? quint16(0xFF00) : quint16(0x0000));
    return pdu;
}

QByteArray encodeWriteSingleRegisterRequest(quint16 address, quint16 value)
{
    QByteArray pdu;
    pdu.append(char(quint8(FunctionCode::WriteSingleRegister)));
    appendBigEndianU16(pdu, address);
    appendBigEndianU16(pdu, value);
    return pdu;
}

QByteArray encodeWriteMultipleCoilsRequest(quint16 startAddress, const QList<bool> &values)
{
    QByteArray pdu;
    pdu.append(char(quint8(FunctionCode::WriteMultipleCoils)));
    appendBigEndianU16(pdu, startAddress);
    appendBigEndianU16(pdu, quint16(values.size()));

    const QByteArray packed = packBits(values);
    pdu.append(char(quint8(packed.size())));
    pdu.append(packed);
    return pdu;
}

QByteArray encodeWriteMultipleRegistersRequest(quint16 startAddress, const QList<quint16> &values)
{
    QByteArray pdu;
    pdu.append(char(quint8(FunctionCode::WriteMultipleRegisters)));
    appendBigEndianU16(pdu, startAddress);
    appendBigEndianU16(pdu, quint16(values.size()));
    pdu.append(char(quint8(values.size() * 2)));
    for (quint16 value : values)
        appendBigEndianU16(pdu, value);
    return pdu;
}

PduDecodeResult<ReadBitsResponse> decodeReadCoilsResponse(const QByteArray &pdu, quint16 requestedQuantity)
{
    return decodeReadBitsResponseImpl(pdu, quint8(FunctionCode::ReadCoils), requestedQuantity);
}

PduDecodeResult<ReadBitsResponse> decodeReadDiscreteInputsResponse(const QByteArray &pdu,
                                                                     quint16 requestedQuantity)
{
    return decodeReadBitsResponseImpl(pdu, quint8(FunctionCode::ReadDiscreteInputs), requestedQuantity);
}

PduDecodeResult<ReadRegistersResponse> decodeReadHoldingRegistersResponse(const QByteArray &pdu)
{
    return decodeReadRegistersResponseImpl(pdu, quint8(FunctionCode::ReadHoldingRegisters));
}

PduDecodeResult<ReadRegistersResponse> decodeReadInputRegistersResponse(const QByteArray &pdu)
{
    return decodeReadRegistersResponseImpl(pdu, quint8(FunctionCode::ReadInputRegisters));
}

PduDecodeResult<WriteSingleResponse> decodeWriteSingleCoilResponse(const QByteArray &pdu)
{
    return decodeWriteSingleResponseImpl(pdu, quint8(FunctionCode::WriteSingleCoil));
}

PduDecodeResult<WriteSingleResponse> decodeWriteSingleRegisterResponse(const QByteArray &pdu)
{
    return decodeWriteSingleResponseImpl(pdu, quint8(FunctionCode::WriteSingleRegister));
}

PduDecodeResult<WriteMultipleResponse> decodeWriteMultipleCoilsResponse(const QByteArray &pdu)
{
    return decodeWriteMultipleResponseImpl(pdu, quint8(FunctionCode::WriteMultipleCoils));
}

PduDecodeResult<WriteMultipleResponse> decodeWriteMultipleRegistersResponse(const QByteArray &pdu)
{
    return decodeWriteMultipleResponseImpl(pdu, quint8(FunctionCode::WriteMultipleRegisters));
}

} // namespace ModbusViewer::Core
