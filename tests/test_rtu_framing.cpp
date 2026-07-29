#include <QByteArray>
#include <QTest>

#include "modbus/ModbusRtuFramer.h"

using namespace ModbusViewer::Core;

class RtuFramingTest : public QObject
{
    Q_OBJECT

private slots:
    void encodeAppendsUnitIdAndCrc();
    void decodeRoundTripsWithEncode();
    void decodeRejectsFrameShorterThanUnitIdPlusCrc();
    void decodeRejectsCorruptedCrc();

    void expectedLengthNeedsFunctionCodeBeforeItCanDecide();
    void expectedLengthForReadResponseUsesByteCountField();
    void expectedLengthForWriteResponseIsFixedEightBytes();
    void expectedLengthForExceptionResponseIsFiveBytes();
    void expectedLengthMatchesActualEncodedFrames();
};

void RtuFramingTest::encodeAppendsUnitIdAndCrc()
{
    const QByteArray pdu = QByteArray::fromHex("030000000A");
    const QByteArray frame = encodeRtuFrame(0x01, pdu);

    // unit id (1) + pdu (pdu.size()) + crc (2)
    QCOMPARE(frame.size(), 1 + pdu.size() + 2);
    QCOMPARE(quint8(frame[0]), quint8(0x01));
    QCOMPARE(frame.mid(1, pdu.size()), pdu);
}

void RtuFramingTest::decodeRoundTripsWithEncode()
{
    const QByteArray pdu = QByteArray::fromHex("06006B0064");
    const QByteArray frame = encodeRtuFrame(0x11, pdu);

    const auto decoded = decodeRtuFrame(frame);
    QVERIFY(decoded.has_value());
    QCOMPARE(decoded->unitId, quint8(0x11));
    QCOMPARE(decoded->pdu, pdu);
}

void RtuFramingTest::decodeRejectsFrameShorterThanUnitIdPlusCrc()
{
    QVERIFY(!decodeRtuFrame(QByteArray::fromHex("01")).has_value());
    QVERIFY(!decodeRtuFrame(QByteArray()).has_value());
}

void RtuFramingTest::decodeRejectsCorruptedCrc()
{
    QByteArray frame = encodeRtuFrame(0x01, QByteArray::fromHex("0300000001"));
    frame[1] = char(frame[1] ^ 0xFF); // flip bits in the pdu, invalidating the CRC

    QVERIFY(!decodeRtuFrame(frame).has_value());
}

void RtuFramingTest::expectedLengthNeedsFunctionCodeBeforeItCanDecide()
{
    QCOMPARE(expectedRtuResponseLength(QByteArray()), 0);
    QCOMPARE(expectedRtuResponseLength(QByteArray::fromHex("01")), 0); // unit id only
}

void RtuFramingTest::expectedLengthForReadResponseUsesByteCountField()
{
    // unit + fc + byteCount not yet present -> undetermined
    QCOMPARE(expectedRtuResponseLength(QByteArray::fromHex("0103")), 0);

    // byte count 6 -> unit(1) + fc(1) + byteCount(1) + data(6) + crc(2) = 11
    QCOMPARE(expectedRtuResponseLength(QByteArray::fromHex("010306")), 11);

    // Same rule applies to coils/discrete inputs/input registers
    QCOMPARE(expectedRtuResponseLength(QByteArray::fromHex("010101")), 6);
    QCOMPARE(expectedRtuResponseLength(QByteArray::fromHex("010402")), 7);
}

void RtuFramingTest::expectedLengthForWriteResponseIsFixedEightBytes()
{
    // unit(1) + fc(1) + address(2) + value-or-quantity(2) + crc(2) = 8
    QCOMPARE(expectedRtuResponseLength(QByteArray::fromHex("0105")), 8);
    QCOMPARE(expectedRtuResponseLength(QByteArray::fromHex("0106")), 8);
    QCOMPARE(expectedRtuResponseLength(QByteArray::fromHex("010F")), 8);
    QCOMPARE(expectedRtuResponseLength(QByteArray::fromHex("0110")), 8);
}

void RtuFramingTest::expectedLengthForExceptionResponseIsFiveBytes()
{
    // unit(1) + fc|0x80(1) + exception code(1) + crc(2) = 5
    QCOMPARE(expectedRtuResponseLength(QByteArray::fromHex("0183")), 5);
}

// The length predictor and the encoder must agree, or a reader will either stall
// waiting for bytes that never come or cut a frame short.
void RtuFramingTest::expectedLengthMatchesActualEncodedFrames()
{
    const QByteArray readResponse = encodeRtuFrame(0x01, QByteArray::fromHex("0306022B00000064"));
    QCOMPARE(expectedRtuResponseLength(readResponse), readResponse.size());

    const QByteArray writeResponse = encodeRtuFrame(0x01, QByteArray::fromHex("0600010003"));
    QCOMPARE(expectedRtuResponseLength(writeResponse), writeResponse.size());

    const QByteArray exceptionResponse = encodeRtuFrame(0x01, QByteArray::fromHex("8302"));
    QCOMPARE(expectedRtuResponseLength(exceptionResponse), exceptionResponse.size());
}

QTEST_APPLESS_MAIN(RtuFramingTest)
#include "test_rtu_framing.moc"
