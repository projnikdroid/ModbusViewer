#include <QByteArray>
#include <QTest>

#include "modbus/ModbusPduCodec.h"

using namespace ModbusViewer::Core;

class PduCodecTest : public QObject
{
    Q_OBJECT

private slots:
    void encodeReadRequestProducesFiveByteRequest();
    void decodeReadHoldingRegistersResponseParsesValues();
    void decodeReadInputRegistersResponseParsesValues();
    void decodeReadCoilsResponseIgnoresPaddingBitsPastRequestedQuantity();
    void decodeReadDiscreteInputsResponseParsesValues();
    void encodeWriteSingleCoilRequestUsesOnOffSentinels();
    void encodeWriteSingleRegisterRequestEncodesValueDirectly();
    void decodeWriteSingleResponseEchoesAddressAndValue();
    void encodeWriteMultipleCoilsRequestPacksBitsWithCorrectByteCount();
    void encodeWriteMultipleRegistersRequestEncodesEachValueBigEndian();
    void decodeWriteMultipleResponseEchoesStartAddressAndQuantity();
    void decodeDetectsExceptionResponseAndDescribesIt();
    void decodeReturnsMalformedForEmptyPdu();
    void decodeReturnsMalformedForUnexpectedFunctionCode();
    void decodeReturnsMalformedWhenPduShorterThanDeclaredByteCount();
};

void PduCodecTest::encodeReadRequestProducesFiveByteRequest()
{
    const QByteArray pdu = encodeReadRequest(FunctionCode::ReadHoldingRegisters, 0x006B, 0x0003);
    QCOMPARE(pdu, QByteArray::fromHex("03006B0003"));
}

void PduCodecTest::decodeReadHoldingRegistersResponseParsesValues()
{
    // FC03, byte count 6, three registers: 0x022B, 0x0000, 0x0064
    const QByteArray pdu = QByteArray::fromHex("0306022B00000064");

    const auto result = decodeReadHoldingRegistersResponse(pdu);
    QVERIFY(result.ok());
    QCOMPARE(result.value.values, (QList<quint16>{0x022B, 0x0000, 0x0064}));
}

void PduCodecTest::decodeReadInputRegistersResponseParsesValues()
{
    const QByteArray pdu = QByteArray::fromHex("0402000A");

    const auto result = decodeReadInputRegistersResponse(pdu);
    QVERIFY(result.ok());
    QCOMPARE(result.value.values, (QList<quint16>{0x000A}));
}

void PduCodecTest::decodeReadCoilsResponseIgnoresPaddingBitsPastRequestedQuantity()
{
    // FC01, byte count 1, byte 0xCD = 1100 1101 -> bits 0..7 = 1,0,1,1,0,0,1,1
    // Only the first 5 were requested; bits 5-7 are padding and must be dropped.
    const QByteArray pdu = QByteArray::fromHex("0101CD");

    const auto result = decodeReadCoilsResponse(pdu, 5);
    QVERIFY(result.ok());
    QCOMPARE(result.value.values, (QList<bool>{true, false, true, true, false}));
}

void PduCodecTest::decodeReadDiscreteInputsResponseParsesValues()
{
    const QByteArray pdu = QByteArray::fromHex("020101");

    const auto result = decodeReadDiscreteInputsResponse(pdu, 1);
    QVERIFY(result.ok());
    QCOMPARE(result.value.values, (QList<bool>{true}));
}

void PduCodecTest::encodeWriteSingleCoilRequestUsesOnOffSentinels()
{
    QCOMPARE(encodeWriteSingleCoilRequest(0x00AC, true), QByteArray::fromHex("0500ACFF00"));
    QCOMPARE(encodeWriteSingleCoilRequest(0x00AC, false), QByteArray::fromHex("0500AC0000"));
}

void PduCodecTest::encodeWriteSingleRegisterRequestEncodesValueDirectly()
{
    QCOMPARE(encodeWriteSingleRegisterRequest(0x0001, 0x0003), QByteArray::fromHex("0600010003"));
}

void PduCodecTest::decodeWriteSingleResponseEchoesAddressAndValue()
{
    const QByteArray coilPdu = QByteArray::fromHex("0500ACFF00");
    const auto coilResult = decodeWriteSingleCoilResponse(coilPdu);
    QVERIFY(coilResult.ok());
    QCOMPARE(coilResult.value.address, quint16(0x00AC));
    QCOMPARE(coilResult.value.value, quint16(0xFF00));

    const QByteArray registerPdu = QByteArray::fromHex("0600010003");
    const auto registerResult = decodeWriteSingleRegisterResponse(registerPdu);
    QVERIFY(registerResult.ok());
    QCOMPARE(registerResult.value.address, quint16(0x0001));
    QCOMPARE(registerResult.value.value, quint16(0x0003));
}

void PduCodecTest::encodeWriteMultipleCoilsRequestPacksBitsWithCorrectByteCount()
{
    const QList<bool> values{true, false, true, true, false, false, true, true, true};
    const QByteArray pdu = encodeWriteMultipleCoilsRequest(0x0013, values);

    // FC + start addr (2) + quantity (2) + byte count (1) + packed bytes (2 for 9 bits)
    QCOMPARE(pdu.size(), 1 + 2 + 2 + 1 + 2);
    QCOMPARE(pdu, QByteArray::fromHex("0F0013000902CD01"));
}

void PduCodecTest::encodeWriteMultipleRegistersRequestEncodesEachValueBigEndian()
{
    const QList<quint16> values{0x000A, 0x0102};
    const QByteArray pdu = encodeWriteMultipleRegistersRequest(0x0001, values);

    QCOMPARE(pdu, QByteArray::fromHex("100001000204000A0102"));
}

void PduCodecTest::decodeWriteMultipleResponseEchoesStartAddressAndQuantity()
{
    const QByteArray coilsPdu = QByteArray::fromHex("0F00130009");
    const auto coilsResult = decodeWriteMultipleCoilsResponse(coilsPdu);
    QVERIFY(coilsResult.ok());
    QCOMPARE(coilsResult.value.startAddress, quint16(0x0013));
    QCOMPARE(coilsResult.value.quantity, quint16(0x0009));

    const QByteArray registersPdu = QByteArray::fromHex("1000010002");
    const auto registersResult = decodeWriteMultipleRegistersResponse(registersPdu);
    QVERIFY(registersResult.ok());
    QCOMPARE(registersResult.value.startAddress, quint16(0x0001));
    QCOMPARE(registersResult.value.quantity, quint16(0x0002));
}

void PduCodecTest::decodeDetectsExceptionResponseAndDescribesIt()
{
    // FC03 | 0x80 = 0x83, exception code 0x02 (Illegal Data Address)
    const QByteArray pdu = QByteArray::fromHex("8302");

    const auto result = decodeReadHoldingRegistersResponse(pdu);
    QCOMPARE(result.status, PduDecodeStatus::ExceptionResponse);
    QVERIFY(!result.ok());
    QCOMPARE(result.exception.originalFunctionCode, quint8(0x03));
    QCOMPARE(result.exception.exceptionCode, ModbusExceptionCode::IllegalDataAddress);
    QCOMPARE(result.exception.message, QStringLiteral("Illegal Data Address"));
    QCOMPARE(result.errorMessage, QStringLiteral("Illegal Data Address"));
}

void PduCodecTest::decodeReturnsMalformedForEmptyPdu()
{
    const auto result = decodeReadHoldingRegistersResponse(QByteArray());
    QCOMPARE(result.status, PduDecodeStatus::MalformedFrame);
}

void PduCodecTest::decodeReturnsMalformedForUnexpectedFunctionCode()
{
    // Asking to decode as holding registers (0x03) but the pdu is actually FC04.
    const QByteArray pdu = QByteArray::fromHex("0402000A");
    const auto result = decodeReadHoldingRegistersResponse(pdu);
    QCOMPARE(result.status, PduDecodeStatus::MalformedFrame);
}

void PduCodecTest::decodeReturnsMalformedWhenPduShorterThanDeclaredByteCount()
{
    // Declares byte count 4 (two registers) but only supplies one register's worth.
    const QByteArray pdu = QByteArray::fromHex("0304022B");
    const auto result = decodeReadHoldingRegistersResponse(pdu);
    QCOMPARE(result.status, PduDecodeStatus::MalformedFrame);
}

QTEST_APPLESS_MAIN(PduCodecTest)
#include "test_pdu_codec.moc"
