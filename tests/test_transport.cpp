#include <QByteArray>
#include <QSignalSpy>
#include <QTest>

#include "support/FakeTransport.h"

using ModbusViewer::Test::FakeTransport;

class TransportContractTest : public QObject
{
    Q_OBJECT

private slots:
    void openEmitsConnectionStateChangedTrueAndReportsOpen();
    void closeEmitsConnectionStateChangedFalseAndReportsClosed();
    void openFailureEmitsErrorAndLeavesTransportClosed();
    void writeRecordsExactBytesWritten();
    void dataReceivedSignalCarriesExactBytes();
    void supportsPipeliningReflectsConfiguration();
};

void TransportContractTest::openEmitsConnectionStateChangedTrueAndReportsOpen()
{
    FakeTransport transport;
    QSignalSpy spy(&transport, &FakeTransport::connectionStateChanged);

    QVERIFY(transport.open());
    QVERIFY(transport.isOpen());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toBool(), true);
}

void TransportContractTest::closeEmitsConnectionStateChangedFalseAndReportsClosed()
{
    FakeTransport transport;
    transport.open();

    QSignalSpy spy(&transport, &FakeTransport::connectionStateChanged);
    transport.close();

    QVERIFY(!transport.isOpen());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toBool(), false);
}

void TransportContractTest::openFailureEmitsErrorAndLeavesTransportClosed()
{
    FakeTransport transport;
    transport.setOpenShouldSucceed(false);
    QSignalSpy errorSpy(&transport, &FakeTransport::errorOccurred);

    QVERIFY(!transport.open());
    QVERIFY(!transport.isOpen());
    QCOMPARE(errorSpy.count(), 1);
}

void TransportContractTest::writeRecordsExactBytesWritten()
{
    FakeTransport transport;
    const QByteArray chunk = QByteArray::fromHex("0103006B0003");

    QCOMPARE(transport.write(chunk), qint64(chunk.size()));
    QCOMPARE(transport.writtenChunks(), (QList<QByteArray>{chunk}));
}

void TransportContractTest::dataReceivedSignalCarriesExactBytes()
{
    FakeTransport transport;
    QSignalSpy spy(&transport, &FakeTransport::dataReceived);

    const QByteArray incoming = QByteArray::fromHex("0306022B00000064");
    transport.simulateDataReceived(incoming);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toByteArray(), incoming);
}

void TransportContractTest::supportsPipeliningReflectsConfiguration()
{
    FakeTransport transport;
    QVERIFY(!transport.supportsPipelining());

    transport.setSupportsPipelining(true);
    QVERIFY(transport.supportsPipelining());
}

QTEST_APPLESS_MAIN(TransportContractTest)
#include "test_transport.moc"
