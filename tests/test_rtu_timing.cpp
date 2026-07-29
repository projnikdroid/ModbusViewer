#include <QTest>

#include "modbus/RtuTiming.h"

using namespace ModbusViewer::Core;

class RtuTimingTest : public QObject
{
    Q_OBJECT

private slots:
    void characterTimeIsElevenBitsPerCharacter();
    void interFrameSilenceIsThreeAndAHalfCharacterTimesAtLowBaudRates();
    void interFrameSilenceIsFixedAboveNineteenTwoHundred_data();
    void interFrameSilenceIsFixedAboveNineteenTwoHundred();
    void slowerBaudRateMeansLongerSilence();
    void invalidBaudRateFallsBackToTheFixedMinimum();
};

// A Modbus RTU character is 11 bits on the wire: 1 start + 8 data + 1 parity +
// 1 stop. Frames with no parity still use 2 stop bits, so the count holds.
void RtuTimingTest::characterTimeIsElevenBitsPerCharacter()
{
    // 9600 baud -> 11 bits takes 11/9600 s = 1145.8 us
    QCOMPARE(characterTimeMicroseconds(9600), 1145);
}

void RtuTimingTest::interFrameSilenceIsThreeAndAHalfCharacterTimesAtLowBaudRates()
{
    // 3.5 chars = 38.5 bits; at 9600 baud that is 38.5/9600 s = 4010.4 us
    QCOMPARE(interFrameSilenceMicroseconds(9600), 4010);

    // At the 19200 boundary: 38.5/19200 s = 2005.2 us
    QCOMPARE(interFrameSilenceMicroseconds(19200), 2005);
}

void RtuTimingTest::interFrameSilenceIsFixedAboveNineteenTwoHundred_data()
{
    QTest::addColumn<int>("baudRate");
    QTest::newRow("38400") << 38400;
    QTest::newRow("57600") << 57600;
    QTest::newRow("115200") << 115200;
    QTest::newRow("230400") << 230400;
}

// The Modbus spec fixes the inter-frame delay at 1.750 ms above 19200 baud rather
// than letting it shrink indefinitely, so faster links do not outrun slave devices.
void RtuTimingTest::interFrameSilenceIsFixedAboveNineteenTwoHundred()
{
    QFETCH(int, baudRate);
    QCOMPARE(interFrameSilenceMicroseconds(baudRate), FixedInterFrameSilenceMicroseconds);
}

void RtuTimingTest::slowerBaudRateMeansLongerSilence()
{
    QVERIFY(interFrameSilenceMicroseconds(1200) > interFrameSilenceMicroseconds(9600));
    QVERIFY(interFrameSilenceMicroseconds(9600) > interFrameSilenceMicroseconds(19200));
}

void RtuTimingTest::invalidBaudRateFallsBackToTheFixedMinimum()
{
    QCOMPARE(interFrameSilenceMicroseconds(0), FixedInterFrameSilenceMicroseconds);
    QCOMPARE(interFrameSilenceMicroseconds(-1), FixedInterFrameSilenceMicroseconds);
}

QTEST_APPLESS_MAIN(RtuTimingTest)
#include "test_rtu_timing.moc"
