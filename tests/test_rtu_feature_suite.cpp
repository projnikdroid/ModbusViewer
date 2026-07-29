#include <QSignalSpy>
#include <QTest>

#include "ConnectionController.h"
#include "modbus/ModbusPduCodec.h"
#include "modbus/ModbusRtuFramer.h"
#include "modbus/ModbusTransactionManager.h"
#include "poll/PollEngine.h"
#include "support/FakeTransport.h"

// End-to-end RTU coverage that test_rtu_framing/test_rtu_timing/
// test_transaction_manager's rtuMode* cases don't reach: the full read-poll loop
// (coalescing, half-duplex single-in-flight enforcement, failure handling) and a
// write, all driven through real CRC-framed wire bytes via FakeTransport rather
// than a positional stand-in. Stops short of ConnectionController::connectToDevice()
// itself -- SerialTransport wraps a real QSerialPort with no injection seam, and
// this machine has no serial hardware to exercise it against (see PROGRESS.md's
// M4 entry). canConnect()'s RTU-readiness check is pure state, so it is covered
// here without touching the transport at all.

using namespace ModbusViewer::Core;
using ModbusViewer::AppLib::ConnectionController;
using ModbusViewer::Test::FakeTransport;

namespace {

PollTarget target(quint16 address, quint16 quantity)
{
    PollTarget t;
    t.unitId = 1;
    t.registerType = RegisterType::HoldingRegister;
    t.startAddress = address;
    t.quantity = quantity;
    return t;
}

QByteArray readRegistersResponsePdu(int registerCount, quint16 fillValue)
{
    QByteArray pdu;
    pdu.append(char(quint8(FunctionCode::ReadHoldingRegisters)));
    pdu.append(char(quint8(registerCount * 2)));
    for (int i = 0; i < registerCount; ++i) {
        pdu.append(char((fillValue >> 8) & 0xFF));
        pdu.append(char(fillValue & 0xFF));
    }
    return pdu;
}

// Answers the RTU-framed (CRC-wrapped) request written at chunkIndex, exactly like
// a real slave would.
void respondOverRtu(FakeTransport &transport, int chunkIndex, quint16 fillValue)
{
    const auto request = decodeRtuFrame(transport.writtenChunks().at(chunkIndex));
    Q_ASSERT(request.has_value());

    // Byte 3..4 of a read request PDU is the quantity.
    const int quantity = (quint8(request->pdu[3]) << 8) | quint8(request->pdu[4]);
    transport.simulateDataReceived(
        encodeRtuFrame(request->unitId, readRegistersResponsePdu(quantity, fillValue)));
}

} // namespace

class RtuFeatureSuiteTest : public QObject
{
    Q_OBJECT

private slots:
    void rtuIsTheDefaultConnectionTypeAndRequiresAPortNameToConnect();
    void pollingOverRtuDecodesCrcFramedResponses();
    void pollingOverRtuNeverExceedsOneRequestInFlight();
    void pollingOverRtuCoalescesNearbyTargetsIntoOneRoundTrip();
    void corruptedCrcDuringPollingTimesOutAndAdvancesTheCycleRatherThanStalling();
    void writeSingleRegisterRoundTripsOverRtuFraming();
};

void RtuFeatureSuiteTest::rtuIsTheDefaultConnectionTypeAndRequiresAPortNameToConnect()
{
    ConnectionController controller;

    QCOMPARE(controller.connectionType(), ConnectionController::ConnectionType::Rtu);
    QVERIFY(!controller.canConnect()); // no port name selected yet

    controller.setPortName(QStringLiteral("COM3"));
    QVERIFY(controller.canConnect());

    controller.setPortName(QString());
    QVERIFY(!controller.canConnect());
}

void RtuFeatureSuiteTest::pollingOverRtuDecodesCrcFramedResponses()
{
    FakeTransport transport;
    transport.setSupportsPipelining(false); // RTU is half-duplex
    ModbusTransactionManager manager(&transport);
    manager.setFramingMode(ModbusTransactionManager::FramingMode::Rtu);
    PollEngine engine(&manager);
    engine.setIntervalMs(10000);
    engine.setTargets({target(100, 3)});
    QSignalSpy valuesSpy(&engine, &PollEngine::targetRegistersUpdated);

    engine.start();
    QCOMPARE(transport.writtenChunks().size(), 1);
    QVERIFY(decodeRtuFrame(transport.writtenChunks().first()).has_value()); // valid CRC

    respondOverRtu(transport, 0, 7);

    QVERIFY(valuesSpy.wait(500));
    QCOMPARE(valuesSpy.first().at(0).toInt(), 0);   // target index
    QCOMPARE(valuesSpy.first().at(1).toInt(), 100); // start address
    QCOMPARE(valuesSpy.first().at(2).value<QList<quint16>>(), (QList<quint16>{7, 7, 7}));
}

void RtuFeatureSuiteTest::pollingOverRtuNeverExceedsOneRequestInFlight()
{
    FakeTransport transport;
    transport.setSupportsPipelining(false);
    ModbusTransactionManager manager(&transport);
    manager.setFramingMode(ModbusTransactionManager::FramingMode::Rtu);
    PollEngine engine(&manager);
    engine.setMaxInFlight(8); // must be silently capped to 1 by the non-pipelining transport
    engine.setIntervalMs(10000);
    // Far apart so coalescing cannot merge them into one request.
    engine.setTargets({target(0, 1), target(1000, 1), target(2000, 1)});
    QCOMPARE(engine.effectiveWindowSize(), 1);

    engine.start();
    QCOMPARE(transport.writtenChunks().size(), 1);

    respondOverRtu(transport, 0, 1);
    QCOMPARE(transport.writtenChunks().size(), 2);

    respondOverRtu(transport, 1, 1);
    QCOMPARE(transport.writtenChunks().size(), 3);
}

void RtuFeatureSuiteTest::pollingOverRtuCoalescesNearbyTargetsIntoOneRoundTrip()
{
    FakeTransport transport;
    transport.setSupportsPipelining(false);
    ModbusTransactionManager manager(&transport);
    manager.setFramingMode(ModbusTransactionManager::FramingMode::Rtu);
    PollEngine engine(&manager);
    engine.setIntervalMs(10000);
    // Three nearby targets fit inside the default gap budget, so one RTU round trip
    // covers all of them -- coalescing is transport-agnostic, but this confirms it
    // still holds once real CRC framing (rather than MBAP) is in the loop.
    engine.setTargets({target(0, 1), target(2, 1), target(4, 1)});
    QCOMPARE(engine.planCount(), 1);
    QSignalSpy valuesSpy(&engine, &PollEngine::targetRegistersUpdated);

    engine.start();
    QCOMPARE(transport.writtenChunks().size(), 1);
    respondOverRtu(transport, 0, 9);

    QVERIFY(valuesSpy.wait(500));
    QCOMPARE(valuesSpy.count(), 3); // every target still got its own update
}

// RTU has no TCP-style transaction id to reject a stale/mismatched response with --
// a corrupted frame is simply indistinguishable from noise and dropped outright
// (see ModbusRtuFramer::decodeRtuFrame). The only thing that can move the cycle
// forward afterwards is the timeout, and it must still do so rather than leaving
// the engine stuck waiting on an answer that will never come.
void RtuFeatureSuiteTest::corruptedCrcDuringPollingTimesOutAndAdvancesTheCycleRatherThanStalling()
{
    FakeTransport transport;
    transport.setSupportsPipelining(false);
    ModbusTransactionManager manager(&transport);
    manager.setFramingMode(ModbusTransactionManager::FramingMode::Rtu);
    manager.setTimeoutMs(20);
    manager.setRetryCount(0);
    PollEngine engine(&manager);
    engine.setMaxInFlight(1);
    engine.setIntervalMs(10);
    engine.setTargets({target(0, 1), target(1000, 1)});
    QSignalSpy failedSpy(&engine, &PollEngine::pollFailed);

    engine.start();
    const auto request = decodeRtuFrame(transport.writtenChunks().first());
    QVERIFY(request.has_value());

    QByteArray corrupted = encodeRtuFrame(request->unitId, readRegistersResponsePdu(1, 5));
    corrupted[3] = char(corrupted[3] ^ 0xFF); // corrupt a data byte, invalidating the CRC
    transport.simulateDataReceived(corrupted);

    QVERIFY(failedSpy.wait(2000));
    QTRY_VERIFY(transport.writtenChunks().size() >= 2); // the cycle moved on to the second target
    QVERIFY(engine.isRunning());
}

void RtuFeatureSuiteTest::writeSingleRegisterRoundTripsOverRtuFraming()
{
    FakeTransport transport;
    ModbusTransactionManager manager(&transport);
    manager.setFramingMode(ModbusTransactionManager::FramingMode::Rtu);
    QSignalSpy responseSpy(&manager, &ModbusTransactionManager::responseReceived);

    const QByteArray requestPdu = encodeWriteSingleRegisterRequest(40, 1234);
    manager.sendRequest(0x01, requestPdu);

    const auto sent = decodeRtuFrame(transport.writtenChunks().first());
    QVERIFY(sent.has_value());
    QCOMPARE(sent->pdu, requestPdu);

    // A real slave echoes the request PDU back verbatim on success.
    transport.simulateDataReceived(encodeRtuFrame(0x01, requestPdu));

    QCOMPARE(responseSpy.count(), 1);
    const auto decoded = decodeWriteSingleRegisterResponse(responseSpy.first().at(1).toByteArray());
    QVERIFY(decoded.ok());
    QCOMPARE(decoded.value.address, quint16(40));
    QCOMPARE(decoded.value.value, quint16(1234));
}

QTEST_GUILESS_MAIN(RtuFeatureSuiteTest)
#include "test_rtu_feature_suite.moc"
