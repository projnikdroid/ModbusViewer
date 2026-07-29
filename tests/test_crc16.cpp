#include <QByteArray>
#include <QTest>

#include "modbus/Crc16.h"

using namespace ModbusViewer::Core;

class Crc16Test : public QObject
{
    Q_OBJECT

private slots:
    void knownVector();
    void appendingCrcToMessageYieldsZeroCrc();
    void emptyInputIsWellDefined();
};

// Well-known example from the Modbus over Serial Line specification: reading 10
// holding registers from slave 1, starting at address 0.
void Crc16Test::knownVector()
{
    const QByteArray message = QByteArray::fromHex("01030000000A");
    QCOMPARE(computeModbusCrc16(message), quint16(0xCDC5));
}

void Crc16Test::appendingCrcToMessageYieldsZeroCrc()
{
    const QByteArray message = QByteArray::fromHex("1103006B0007");
    const quint16 crc = computeModbusCrc16(message);

    QByteArray withCrcAppended = message;
    withCrcAppended.append(char(crc & 0xFF));        // CRC low byte first
    withCrcAppended.append(char((crc >> 8) & 0xFF)); // then CRC high byte

    QCOMPARE(computeModbusCrc16(withCrcAppended), quint16(0));
}

void Crc16Test::emptyInputIsWellDefined()
{
    QCOMPARE(computeModbusCrc16(QByteArray()), quint16(0xFFFF));
}

QTEST_APPLESS_MAIN(Crc16Test)
#include "test_crc16.moc"
