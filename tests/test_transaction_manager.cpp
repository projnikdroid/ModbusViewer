#include <QByteArray>
#include <QSignalSpy>
#include <QTest>

#include "modbus/ModbusRtuFramer.h"
#include "modbus/ModbusTcpFramer.h"
#include "modbus/ModbusTransactionManager.h"
#include "support/FakeTransport.h"

using namespace ModbusViewer::Core;
using ModbusViewer::Test::FakeTransport;

class TransactionManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void sendRequestWrapsRequestPduInMbapFrame();
    void matchingResponseCompletesRequestAndEmitsPdu();
    void mismatchedTransactionIdIsIgnored();
    void timeoutWithNoRetriesEmitsRequestFailed();
    void timeoutRetriesUpToConfiguredCountThenFails();
    void successAfterOneRetryStillCompletes();

    void rtuModeWrapsRequestInRtuFrameWithCrc();
    void rtuModeCompletesOnMatchingResponse();
    void rtuModeReassemblesResponseArrivingInPieces();
    void rtuModeIgnoresResponseWithBadCrc();

    void multipleRequestsCanBeOutstandingAtOnce();
    void outOfOrderResponsesAreMatchedByTransactionId();
    void correlationIdIsEchoedBackWithTheResponse();
    void onlyTheTimedOutRequestFailsWhileOthersRemain();
    void cancelAllDropsOutstandingRequestsSilently();

    void sendRequestEmitsFrameSentWithRawWireBytes();
    void matchingResponseEmitsFrameReceivedWithRawWireBytes();
    void retransmitOnTimeoutAlsoEmitsFrameSent();
};

void TransactionManagerTest::sendRequestWrapsRequestPduInMbapFrame()
{
    FakeTransport transport;
    ModbusTransactionManager manager(&transport);

    const QByteArray requestPdu = QByteArray::fromHex("03006B0003");
    manager.sendRequest(0x01, requestPdu);

    QCOMPARE(transport.writtenChunks().size(), 1);
    const auto decoded = decodeMbapFrame(transport.writtenChunks().first());
    QVERIFY(decoded.has_value());
    QCOMPARE(decoded->unitId, quint8(0x01));
    QCOMPARE(decoded->pdu, requestPdu);
    QVERIFY(manager.hasRequestInFlight());
}

void TransactionManagerTest::matchingResponseCompletesRequestAndEmitsPdu()
{
    FakeTransport transport;
    ModbusTransactionManager manager(&transport);
    QSignalSpy responseSpy(&manager, &ModbusTransactionManager::responseReceived);

    manager.sendRequest(0x01, QByteArray::fromHex("03006B0003"));
    const auto sentFrame = decodeMbapFrame(transport.writtenChunks().first());
    QVERIFY(sentFrame.has_value());

    const QByteArray responsePdu = QByteArray::fromHex("0306022B00000064");
    transport.simulateDataReceived(encodeMbapFrame(sentFrame->transactionId, 0x01, responsePdu));

    QCOMPARE(responseSpy.count(), 1);
    QCOMPARE(responseSpy.first().at(1).toByteArray(), responsePdu);
    QVERIFY(!manager.hasRequestInFlight());
}

void TransactionManagerTest::mismatchedTransactionIdIsIgnored()
{
    FakeTransport transport;
    ModbusTransactionManager manager(&transport);
    QSignalSpy responseSpy(&manager, &ModbusTransactionManager::responseReceived);

    manager.sendRequest(0x01, QByteArray::fromHex("03006B0003"));

    // A stale response carrying some other transaction id must not complete the
    // request in flight.
    transport.simulateDataReceived(encodeMbapFrame(0xFFFF, 0x01, QByteArray::fromHex("0306022B00000064")));

    QCOMPARE(responseSpy.count(), 0);
    QVERIFY(manager.hasRequestInFlight());
}

void TransactionManagerTest::timeoutWithNoRetriesEmitsRequestFailed()
{
    FakeTransport transport;
    ModbusTransactionManager manager(&transport);
    manager.setTimeoutMs(30);
    manager.setRetryCount(0);
    QSignalSpy failedSpy(&manager, &ModbusTransactionManager::requestFailed);

    manager.sendRequest(0x01, QByteArray::fromHex("03006B0003"));

    QVERIFY(failedSpy.wait(1000));
    QCOMPARE(failedSpy.count(), 1);
    QVERIFY(!manager.hasRequestInFlight());
    QCOMPARE(transport.writtenChunks().size(), 1); // no retry means only the original send
}

void TransactionManagerTest::timeoutRetriesUpToConfiguredCountThenFails()
{
    FakeTransport transport;
    ModbusTransactionManager manager(&transport);
    manager.setTimeoutMs(30);
    manager.setRetryCount(2);
    QSignalSpy failedSpy(&manager, &ModbusTransactionManager::requestFailed);

    manager.sendRequest(0x01, QByteArray::fromHex("03006B0003"));

    QVERIFY(failedSpy.wait(2000));
    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(transport.writtenChunks().size(), 3); // original + 2 retries
}

void TransactionManagerTest::successAfterOneRetryStillCompletes()
{
    FakeTransport transport;
    ModbusTransactionManager manager(&transport);
    manager.setTimeoutMs(50);
    manager.setRetryCount(2);
    QSignalSpy responseSpy(&manager, &ModbusTransactionManager::responseReceived);

    manager.sendRequest(0x01, QByteArray::fromHex("03006B0003"));

    // Let the first attempt time out and exactly one retry go out (at ~50ms), but
    // answer before the second retry would fire (at ~100ms).
    QTest::qWait(70);
    QCOMPARE(transport.writtenChunks().size(), 2);

    const auto retryFrame = decodeMbapFrame(transport.writtenChunks().last());
    QVERIFY(retryFrame.has_value());
    transport.simulateDataReceived(
        encodeMbapFrame(retryFrame->transactionId, 0x01, QByteArray::fromHex("0306022B00000064")));

    QCOMPARE(responseSpy.count(), 1);
}

void TransactionManagerTest::rtuModeWrapsRequestInRtuFrameWithCrc()
{
    FakeTransport transport;
    ModbusTransactionManager manager(&transport);
    manager.setFramingMode(ModbusTransactionManager::FramingMode::Rtu);

    const QByteArray requestPdu = QByteArray::fromHex("03006B0003");
    manager.sendRequest(0x11, requestPdu);

    QCOMPARE(transport.writtenChunks().size(), 1);
    const auto decoded = decodeRtuFrame(transport.writtenChunks().first());
    QVERIFY(decoded.has_value()); // implies the CRC was valid
    QCOMPARE(decoded->unitId, quint8(0x11));
    QCOMPARE(decoded->pdu, requestPdu);
}

void TransactionManagerTest::rtuModeCompletesOnMatchingResponse()
{
    FakeTransport transport;
    ModbusTransactionManager manager(&transport);
    manager.setFramingMode(ModbusTransactionManager::FramingMode::Rtu);
    QSignalSpy responseSpy(&manager, &ModbusTransactionManager::responseReceived);

    manager.sendRequest(0x01, QByteArray::fromHex("03006B0003"));

    const QByteArray responsePdu = QByteArray::fromHex("0306022B00000064");
    transport.simulateDataReceived(encodeRtuFrame(0x01, responsePdu));

    QCOMPARE(responseSpy.count(), 1);
    QCOMPARE(responseSpy.first().at(1).toByteArray(), responsePdu);
    QVERIFY(!manager.hasRequestInFlight());
}

// A serial port delivers bytes as they arrive, not in whole frames.
void TransactionManagerTest::rtuModeReassemblesResponseArrivingInPieces()
{
    FakeTransport transport;
    ModbusTransactionManager manager(&transport);
    manager.setFramingMode(ModbusTransactionManager::FramingMode::Rtu);
    QSignalSpy responseSpy(&manager, &ModbusTransactionManager::responseReceived);

    manager.sendRequest(0x01, QByteArray::fromHex("03006B0003"));

    const QByteArray responsePdu = QByteArray::fromHex("0306022B00000064");
    const QByteArray frame = encodeRtuFrame(0x01, responsePdu);

    transport.simulateDataReceived(frame.left(2)); // unit + function code only
    QCOMPARE(responseSpy.count(), 0);
    transport.simulateDataReceived(frame.mid(2, 3));
    QCOMPARE(responseSpy.count(), 0);
    transport.simulateDataReceived(frame.mid(5));

    QCOMPARE(responseSpy.count(), 1);
    QCOMPARE(responseSpy.first().at(1).toByteArray(), responsePdu);
}

void TransactionManagerTest::rtuModeIgnoresResponseWithBadCrc()
{
    FakeTransport transport;
    ModbusTransactionManager manager(&transport);
    manager.setFramingMode(ModbusTransactionManager::FramingMode::Rtu);
    manager.setTimeoutMs(30);
    manager.setRetryCount(0);
    QSignalSpy responseSpy(&manager, &ModbusTransactionManager::responseReceived);
    QSignalSpy failedSpy(&manager, &ModbusTransactionManager::requestFailed);

    manager.sendRequest(0x01, QByteArray::fromHex("03006B0003"));

    QByteArray corrupted = encodeRtuFrame(0x01, QByteArray::fromHex("0306022B00000064"));
    corrupted[4] = char(corrupted[4] ^ 0xFF); // corrupt a data byte, invalidating the CRC
    transport.simulateDataReceived(corrupted);

    QCOMPARE(responseSpy.count(), 0);
    QVERIFY(failedSpy.wait(1000)); // corrupted frame is dropped; the timeout fires instead
}

void TransactionManagerTest::multipleRequestsCanBeOutstandingAtOnce()
{
    FakeTransport transport;
    transport.setSupportsPipelining(true);
    ModbusTransactionManager manager(&transport);

    manager.sendRequest(0x01, QByteArray::fromHex("0300000001"), 10);
    manager.sendRequest(0x01, QByteArray::fromHex("0300100001"), 20);
    manager.sendRequest(0x01, QByteArray::fromHex("0300200001"), 30);

    QCOMPARE(transport.writtenChunks().size(), 3);
    QCOMPARE(manager.inFlightCount(), 3);
    QVERIFY(manager.supportsPipelining());
}

// The whole reason Modbus TCP can pipeline: the transaction id makes ordering
// irrelevant, so a device answering its queue out of order is still correct.
void TransactionManagerTest::outOfOrderResponsesAreMatchedByTransactionId()
{
    FakeTransport transport;
    transport.setSupportsPipelining(true);
    ModbusTransactionManager manager(&transport);
    QSignalSpy responseSpy(&manager, &ModbusTransactionManager::responseReceived);

    manager.sendRequest(0x01, QByteArray::fromHex("0300000001"), 111);
    manager.sendRequest(0x01, QByteArray::fromHex("0300100001"), 222);

    const auto first = decodeMbapFrame(transport.writtenChunks().at(0));
    const auto second = decodeMbapFrame(transport.writtenChunks().at(1));
    QVERIFY(first.has_value() && second.has_value());

    // Answer the second request first.
    transport.simulateDataReceived(encodeMbapFrame(second->transactionId, 0x01, QByteArray::fromHex("030200FF")));
    transport.simulateDataReceived(encodeMbapFrame(first->transactionId, 0x01, QByteArray::fromHex("030200AA")));

    QCOMPARE(responseSpy.count(), 2);
    QCOMPARE(responseSpy.at(0).at(0).toULongLong(), quint64(222));
    QCOMPARE(responseSpy.at(1).at(0).toULongLong(), quint64(111));
    QCOMPARE(manager.inFlightCount(), 0);
}

void TransactionManagerTest::correlationIdIsEchoedBackWithTheResponse()
{
    FakeTransport transport;
    ModbusTransactionManager manager(&transport);
    QSignalSpy responseSpy(&manager, &ModbusTransactionManager::responseReceived);

    manager.sendRequest(0x01, QByteArray::fromHex("0300000001"), 0xDEADBEEF);
    const auto sent = decodeMbapFrame(transport.writtenChunks().first());
    transport.simulateDataReceived(encodeMbapFrame(sent->transactionId, 0x01, QByteArray::fromHex("030200AA")));

    QCOMPARE(responseSpy.count(), 1);
    QCOMPARE(responseSpy.first().at(0).toULongLong(), quint64(0xDEADBEEF));
}

void TransactionManagerTest::onlyTheTimedOutRequestFailsWhileOthersRemain()
{
    FakeTransport transport;
    transport.setSupportsPipelining(true);
    ModbusTransactionManager manager(&transport);
    manager.setTimeoutMs(40);
    manager.setRetryCount(0);
    QSignalSpy failedSpy(&manager, &ModbusTransactionManager::requestFailed);
    QSignalSpy responseSpy(&manager, &ModbusTransactionManager::responseReceived);

    manager.sendRequest(0x01, QByteArray::fromHex("0300000001"), 1);
    manager.sendRequest(0x01, QByteArray::fromHex("0300100001"), 2);

    // Answer only the second; the first must expire on its own without disturbing it.
    const auto second = decodeMbapFrame(transport.writtenChunks().at(1));
    transport.simulateDataReceived(encodeMbapFrame(second->transactionId, 0x01, QByteArray::fromHex("030200FF")));
    QCOMPARE(responseSpy.count(), 1);

    QVERIFY(failedSpy.wait(1000));
    QCOMPARE(failedSpy.count(), 1);
    QCOMPARE(failedSpy.first().at(0).toULongLong(), quint64(1));
    QCOMPARE(manager.inFlightCount(), 0);
}

void TransactionManagerTest::cancelAllDropsOutstandingRequestsSilently()
{
    FakeTransport transport;
    transport.setSupportsPipelining(true);
    ModbusTransactionManager manager(&transport);
    manager.setTimeoutMs(20);
    manager.setRetryCount(0);
    QSignalSpy failedSpy(&manager, &ModbusTransactionManager::requestFailed);

    manager.sendRequest(0x01, QByteArray::fromHex("0300000001"), 1);
    manager.sendRequest(0x01, QByteArray::fromHex("0300100001"), 2);
    manager.cancelAll();

    QCOMPARE(manager.inFlightCount(), 0);
    QTest::qWait(60); // abandoning work is not a failure worth reporting
    QCOMPARE(failedSpy.count(), 0);
}

void TransactionManagerTest::sendRequestEmitsFrameSentWithRawWireBytes()
{
    FakeTransport transport;
    ModbusTransactionManager manager(&transport);
    QSignalSpy sentSpy(&manager, &ModbusTransactionManager::frameSent);

    const QByteArray requestPdu = QByteArray::fromHex("03006B0003");
    manager.sendRequest(0x01, requestPdu);

    QCOMPARE(sentSpy.count(), 1);
    const auto decoded = decodeMbapFrame(sentSpy.first().at(0).toByteArray());
    QVERIFY(decoded.has_value());
    QCOMPARE(decoded->unitId, quint8(0x01));
    QCOMPARE(decoded->pdu, requestPdu);
}

void TransactionManagerTest::matchingResponseEmitsFrameReceivedWithRawWireBytes()
{
    FakeTransport transport;
    ModbusTransactionManager manager(&transport);
    QSignalSpy receivedSpy(&manager, &ModbusTransactionManager::frameReceived);

    manager.sendRequest(0x01, QByteArray::fromHex("03006B0003"));
    const auto sentFrame = decodeMbapFrame(transport.writtenChunks().first());
    QVERIFY(sentFrame.has_value());

    const QByteArray responsePdu = QByteArray::fromHex("0306022B00000064");
    const QByteArray responseFrame = encodeMbapFrame(sentFrame->transactionId, 0x01, responsePdu);
    transport.simulateDataReceived(responseFrame);

    QCOMPARE(receivedSpy.count(), 1);
    QCOMPARE(receivedSpy.first().at(0).toByteArray(), responseFrame);
}

void TransactionManagerTest::retransmitOnTimeoutAlsoEmitsFrameSent()
{
    FakeTransport transport;
    ModbusTransactionManager manager(&transport);
    manager.setTimeoutMs(30);
    manager.setRetryCount(1);
    QSignalSpy sentSpy(&manager, &ModbusTransactionManager::frameSent);

    manager.sendRequest(0x01, QByteArray::fromHex("03006B0003"));
    QCOMPARE(sentSpy.count(), 1); // the original send

    QTest::qWait(70); // long enough for the single retry to fire
    QCOMPARE(sentSpy.count(), 2);
    QCOMPARE(transport.writtenChunks().size(), 2);
    QCOMPARE(sentSpy.last().at(0).toByteArray(), transport.writtenChunks().last());
}

QTEST_GUILESS_MAIN(TransactionManagerTest)
#include "test_transaction_manager.moc"
