#include <QByteArray>
#include <QTest>

#include "modbus/ModbusTcpFramer.h"

using namespace ModbusViewer::Core;

class MbapFramingTest : public QObject
{
    Q_OBJECT

private slots:
    void encodeProducesSevenByteHeaderPlusPdu();
    void decodeRoundTripsWithEncode();
    void decodeReturnsNulloptWhenBufferIncomplete();
    void decodeReturnsNulloptForNonZeroProtocolId();
    void decodeIgnoresTrailingBytesAfterOneCompleteFrame();
};

void MbapFramingTest::encodeProducesSevenByteHeaderPlusPdu()
{
    const QByteArray pdu = QByteArray::fromHex("030000000A");
    const QByteArray frame = encodeMbapFrame(0x0007, 0x01, pdu);

    QCOMPARE(frame.size(), MbapHeaderByteCount + pdu.size());
    QCOMPARE(frame.left(2), QByteArray::fromHex("0007")); // transaction id, big-endian
    QCOMPARE(frame.mid(2, 2), QByteArray::fromHex("0000")); // protocol id
    QCOMPARE(frame.mid(4, 2), QByteArray::fromHex("0006")); // length = unit id (1) + pdu (5)
    QCOMPARE(quint8(frame[6]), quint8(0x01));               // unit id
    QCOMPARE(frame.mid(MbapHeaderByteCount), pdu);
}

void MbapFramingTest::decodeRoundTripsWithEncode()
{
    const QByteArray pdu = QByteArray::fromHex("10006B00020400640028");
    const QByteArray frame = encodeMbapFrame(0x1234, 0x11, pdu);

    const auto decoded = decodeMbapFrame(frame);
    QVERIFY(decoded.has_value());
    QCOMPARE(decoded->transactionId, quint16(0x1234));
    QCOMPARE(decoded->unitId, quint8(0x11));
    QCOMPARE(decoded->pdu, pdu);
    QCOMPARE(decoded->frameByteCount, frame.size());
}

void MbapFramingTest::decodeReturnsNulloptWhenBufferIncomplete()
{
    const QByteArray fullFrame = encodeMbapFrame(1, 1, QByteArray::fromHex("030000000A"));

    QVERIFY(!decodeMbapFrame(fullFrame.left(3)).has_value());              // header itself incomplete
    QVERIFY(!decodeMbapFrame(fullFrame.left(fullFrame.size() - 1)).has_value()); // pdu truncated by one byte
}

void MbapFramingTest::decodeReturnsNulloptForNonZeroProtocolId()
{
    QByteArray frame = encodeMbapFrame(1, 1, QByteArray::fromHex("03"));
    frame[3] = char(0x01); // corrupt protocol id low byte

    QVERIFY(!decodeMbapFrame(frame).has_value());
}

void MbapFramingTest::decodeIgnoresTrailingBytesAfterOneCompleteFrame()
{
    const QByteArray pdu = QByteArray::fromHex("030000000A");
    QByteArray buffer = encodeMbapFrame(1, 1, pdu);
    buffer.append(QByteArray::fromHex("AABBCC")); // start of a second, not-yet-complete frame

    const auto decoded = decodeMbapFrame(buffer);
    QVERIFY(decoded.has_value());
    QCOMPARE(decoded->pdu, pdu);
    QCOMPARE(decoded->frameByteCount, MbapHeaderByteCount + pdu.size()); // excludes the trailing bytes
}

QTEST_APPLESS_MAIN(MbapFramingTest)
#include "test_mbap_framing.moc"
