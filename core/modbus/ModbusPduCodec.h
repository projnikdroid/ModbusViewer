#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QtGlobal>

#include "ModbusException.h"

namespace ModbusViewer::Core {

enum class FunctionCode : quint8 {
    ReadCoils = 0x01,
    ReadDiscreteInputs = 0x02,
    ReadHoldingRegisters = 0x03,
    ReadInputRegisters = 0x04,
    WriteSingleCoil = 0x05,
    WriteSingleRegister = 0x06,
    WriteMultipleCoils = 0x0F,
    WriteMultipleRegisters = 0x10,
};

enum class PduDecodeStatus {
    Ok,
    MalformedFrame,
    ExceptionResponse,
};

// Every PDU decode function returns one of these instead of throwing or using an
// out-param error code: a well-formed device exception is a distinct, expected
// outcome from a malformed/corrupted frame, and callers (the comms log, the
// register error state) care about the difference.
template <typename T>
struct PduDecodeResult
{
    PduDecodeStatus status = PduDecodeStatus::MalformedFrame;
    T value{};
    ModbusExceptionResponse exception{};
    QString errorMessage;

    bool ok() const { return status == PduDecodeStatus::Ok; }

    static PduDecodeResult success(T decodedValue)
    {
        PduDecodeResult result;
        result.status = PduDecodeStatus::Ok;
        result.value = std::move(decodedValue);
        return result;
    }

    static PduDecodeResult malformed(QString message)
    {
        PduDecodeResult result;
        result.status = PduDecodeStatus::MalformedFrame;
        result.errorMessage = std::move(message);
        return result;
    }

    static PduDecodeResult exceptionResponse(ModbusExceptionResponse response)
    {
        PduDecodeResult result;
        result.status = PduDecodeStatus::ExceptionResponse;
        result.errorMessage = response.message;
        result.exception = std::move(response);
        return result;
    }
};

struct ReadBitsResponse
{
    QList<bool> values;
};

struct ReadRegistersResponse
{
    QList<quint16> values;
};

struct WriteSingleResponse
{
    quint16 address = 0;
    quint16 value = 0;
};

struct WriteMultipleResponse
{
    quint16 startAddress = 0;
    quint16 quantity = 0;
};

// --- Requests ---

QByteArray encodeReadRequest(FunctionCode functionCode, quint16 startAddress, quint16 quantity);
QByteArray encodeWriteSingleCoilRequest(quint16 address, bool value);
QByteArray encodeWriteSingleRegisterRequest(quint16 address, quint16 value);
QByteArray encodeWriteMultipleCoilsRequest(quint16 startAddress, const QList<bool> &values);
QByteArray encodeWriteMultipleRegistersRequest(quint16 startAddress, const QList<quint16> &values);

// --- Responses ---
// requestedQuantity is needed only for the bit-packed responses (coils/discrete
// inputs): the wire format only carries a byte count, and the last byte may have
// padding bits beyond the requested quantity that must not be reported as data.

PduDecodeResult<ReadBitsResponse> decodeReadCoilsResponse(const QByteArray &pdu, quint16 requestedQuantity);
PduDecodeResult<ReadBitsResponse> decodeReadDiscreteInputsResponse(const QByteArray &pdu, quint16 requestedQuantity);
PduDecodeResult<ReadRegistersResponse> decodeReadHoldingRegistersResponse(const QByteArray &pdu);
PduDecodeResult<ReadRegistersResponse> decodeReadInputRegistersResponse(const QByteArray &pdu);
PduDecodeResult<WriteSingleResponse> decodeWriteSingleCoilResponse(const QByteArray &pdu);
PduDecodeResult<WriteSingleResponse> decodeWriteSingleRegisterResponse(const QByteArray &pdu);
PduDecodeResult<WriteMultipleResponse> decodeWriteMultipleCoilsResponse(const QByteArray &pdu);
PduDecodeResult<WriteMultipleResponse> decodeWriteMultipleRegistersResponse(const QByteArray &pdu);

} // namespace ModbusViewer::Core
