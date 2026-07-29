#include <QSet>
#include <QSignalSpy>
#include <QTest>

#include "modbus/ModbusPduCodec.h"
#include "modbus/ModbusTcpFramer.h"
#include "modbus/ModbusTransactionManager.h"
#include "poll/PollEngine.h"
#include "support/FakeTransport.h"

using namespace ModbusViewer::Core;
using ModbusViewer::Test::FakeTransport;

namespace {

// Comfortably longer than PollEngine's flush interval, so a flush that was going to
// happen has definitely happened.
constexpr int FlushWaitMs = 100;

PollTarget target(quint16 address, quint16 quantity,
                  RegisterType type = RegisterType::HoldingRegister)
{
    PollTarget t;
    t.unitId = 1;
    t.registerType = type;
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

// Answers the request written at chunkIndex with registers all set to fillValue.
void respondTo(FakeTransport &transport, int chunkIndex, quint16 fillValue)
{
    const auto request = decodeMbapFrame(transport.writtenChunks().at(chunkIndex));
    Q_ASSERT(request.has_value());

    // Byte 3..4 of a read request PDU is the quantity.
    const int quantity = (quint8(request->pdu[3]) << 8) | quint8(request->pdu[4]);
    transport.simulateDataReceived(encodeMbapFrame(request->transactionId, request->unitId,
                                                    readRegistersResponsePdu(quantity, fillValue)));
}

void respondToAllOutstanding(FakeTransport &transport, int fromChunk, quint16 fillValue)
{
    for (int i = fromChunk; i < transport.writtenChunks().size(); ++i)
        respondTo(transport, i, fillValue);
}

// A Modbus exception response (function code's high bit set) so the response
// decodes but fails, exercising applyPlanResponse's decode-failure branch
// rather than a request timeout.
void respondWithException(FakeTransport &transport, int chunkIndex)
{
    const auto request = decodeMbapFrame(transport.writtenChunks().at(chunkIndex));
    Q_ASSERT(request.has_value());

    QByteArray exceptionPdu;
    exceptionPdu.append(char(quint8(FunctionCode::ReadHoldingRegisters) | 0x80));
    exceptionPdu.append(char(0x02)); // Illegal Data Address
    transport.simulateDataReceived(
        encodeMbapFrame(request->transactionId, request->unitId, exceptionPdu));
}

} // namespace

class PollEngineTest : public QObject
{
    Q_OBJECT

private slots:
    void startIssuesAFirstRequestImmediately();
    void stopHaltsFurtherPolling();
    void valuesFromResponseAreEmittedAfterFlush();
    void pipeliningTransportIssuesUpToWindowSizeConcurrently();
    void nonPipeliningTransportIssuesOneRequestAtATime();
    void nextRequestIsIssuedAsEachResponseArrives();
    void cycleCompletesOnlyAfterEveryPlanSettles();
    void updatesAreBatchedRatherThanEmittedPerResponse();
    void changingTargetsDiscardsInFlightResponsesFromTheOldSet();
    void retargetingAfterACycleCompletesDoesNotLeaveTheOldIntervalTimerArmed();
    void multipleTargetsAreCoalescedIntoFewerRequests();
    void failedRequestDoesNotStallTheCycle();

    void timedOutRequestEmitsTargetFailedForItsTarget();
    void decodeFailureEmitsTargetFailedForItsTarget();
    void coalescedFailureEmitsTargetFailedForEveryCoveredTarget();
};

void PollEngineTest::startIssuesAFirstRequestImmediately()
{
    FakeTransport transport;
    ModbusTransactionManager manager(&transport);
    PollEngine engine(&manager);
    engine.setTargets({target(0, 5)});
    engine.setIntervalMs(10);

    engine.start();

    QCOMPARE(transport.writtenChunks().size(), 1);
    QVERIFY(engine.isRunning());
}

void PollEngineTest::stopHaltsFurtherPolling()
{
    FakeTransport transport;
    ModbusTransactionManager manager(&transport);
    PollEngine engine(&manager);
    engine.setTargets({target(0, 2)});
    engine.setIntervalMs(10);

    engine.start();
    respondTo(transport, 0, 1);
    engine.stop();

    QVERIFY(!engine.isRunning());
    const int countAfterStop = transport.writtenChunks().size();
    QTest::qWait(50);
    QCOMPARE(transport.writtenChunks().size(), countAfterStop);
}

void PollEngineTest::valuesFromResponseAreEmittedAfterFlush()
{
    FakeTransport transport;
    ModbusTransactionManager manager(&transport);
    PollEngine engine(&manager);
    engine.setTargets({target(100, 3)});
    engine.setIntervalMs(1000);
    QSignalSpy valuesSpy(&engine, &PollEngine::targetRegistersUpdated);

    engine.start();
    respondTo(transport, 0, 7);

    QVERIFY(valuesSpy.wait(500));
    QCOMPARE(valuesSpy.first().at(0).toInt(), 0);   // target index
    QCOMPARE(valuesSpy.first().at(1).toInt(), 100); // start address
    QCOMPARE(valuesSpy.first().at(2).value<QList<quint16>>(), (QList<quint16>{7, 7, 7}));
}

void PollEngineTest::pipeliningTransportIssuesUpToWindowSizeConcurrently()
{
    FakeTransport transport;
    transport.setSupportsPipelining(true);
    ModbusTransactionManager manager(&transport);
    PollEngine engine(&manager);
    engine.setMaxInFlight(3);

    // Far apart so coalescing cannot merge them: five separate plans.
    engine.setTargets({target(0, 1), target(1000, 1), target(2000, 1), target(3000, 1), target(4000, 1)});
    QCOMPARE(engine.planCount(), 5);
    QCOMPARE(engine.effectiveWindowSize(), 3);

    engine.start();

    // Window of three, so only three go out before any answer comes back.
    QCOMPARE(transport.writtenChunks().size(), 3);
}

// RTU is half-duplex and has no transaction id, so overlapping requests are not
// merely inefficient, they are unanswerable.
void PollEngineTest::nonPipeliningTransportIssuesOneRequestAtATime()
{
    FakeTransport transport;
    transport.setSupportsPipelining(false);
    ModbusTransactionManager manager(&transport);
    PollEngine engine(&manager);
    engine.setMaxInFlight(8);
    engine.setTargets({target(0, 1), target(1000, 1), target(2000, 1)});

    QCOMPARE(engine.effectiveWindowSize(), 1);
    engine.start();
    QCOMPARE(transport.writtenChunks().size(), 1);
}

void PollEngineTest::nextRequestIsIssuedAsEachResponseArrives()
{
    FakeTransport transport;
    transport.setSupportsPipelining(true);
    ModbusTransactionManager manager(&transport);
    PollEngine engine(&manager);
    engine.setMaxInFlight(2);
    engine.setIntervalMs(10000); // keep the next cycle out of the way
    engine.setTargets({target(0, 1), target(1000, 1), target(2000, 1), target(3000, 1)});

    engine.start();
    QCOMPARE(transport.writtenChunks().size(), 2);

    respondTo(transport, 0, 1);
    QCOMPARE(transport.writtenChunks().size(), 3); // window refilled

    respondTo(transport, 1, 1);
    QCOMPARE(transport.writtenChunks().size(), 4);
}

void PollEngineTest::cycleCompletesOnlyAfterEveryPlanSettles()
{
    FakeTransport transport;
    transport.setSupportsPipelining(true);
    ModbusTransactionManager manager(&transport);
    PollEngine engine(&manager);
    engine.setMaxInFlight(4);
    engine.setIntervalMs(10000);
    engine.setTargets({target(0, 1), target(1000, 1), target(2000, 1)});
    QSignalSpy cycleSpy(&engine, &PollEngine::pollCycleCompleted);

    engine.start();
    respondTo(transport, 0, 1);
    respondTo(transport, 1, 1);
    QCOMPARE(cycleSpy.count(), 0); // one still outstanding

    respondTo(transport, 2, 1);
    QCOMPARE(cycleSpy.count(), 1);
}

// The backpressure guarantee: signal volume is bounded by the flush rate, not by how
// fast the device answers.
void PollEngineTest::updatesAreBatchedRatherThanEmittedPerResponse()
{
    FakeTransport transport;
    transport.setSupportsPipelining(true);
    ModbusTransactionManager manager(&transport);
    PollEngine engine(&manager);
    engine.setIntervalMs(0); // poll as fast as the event loop allows
    engine.setTargets({target(0, 4)});
    QSignalSpy valuesSpy(&engine, &PollEngine::targetRegistersUpdated);

    engine.start();

    int responsesDelivered = 0;
    for (int round = 0; round < 40; ++round) {
        const int chunkCount = transport.writtenChunks().size();
        if (chunkCount > responsesDelivered) {
            respondTo(transport, responsesDelivered, quint16(round));
            ++responsesDelivered;
        }
        QCoreApplication::processEvents();
    }

    QVERIFY(responsesDelivered > 10); // the loop really did churn
    // Far fewer emissions than responses: intermediate values were coalesced away.
    QVERIFY2(valuesSpy.count() < responsesDelivered,
             qPrintable(QStringLiteral("emitted %1 updates for %2 responses")
                            .arg(valuesSpy.count())
                            .arg(responsesDelivered)));
}

void PollEngineTest::changingTargetsDiscardsInFlightResponsesFromTheOldSet()
{
    FakeTransport transport;
    transport.setSupportsPipelining(true);
    ModbusTransactionManager manager(&transport);
    PollEngine engine(&manager);
    engine.setIntervalMs(10000);
    engine.setTargets({target(0, 2)});
    QSignalSpy valuesSpy(&engine, &PollEngine::targetRegistersUpdated);

    engine.start();
    const QByteArray staleRequest = transport.writtenChunks().first();

    // Swap targets while that first request is still unanswered.
    engine.setTargets({target(500, 2)});

    // The device finally answers the abandoned request; it must not populate the
    // new target set.
    const auto stale = decodeMbapFrame(staleRequest);
    transport.simulateDataReceived(
        encodeMbapFrame(stale->transactionId, stale->unitId, readRegistersResponsePdu(2, 0xAAAA)));

    QTest::qWait(FlushWaitMs);
    QCOMPARE(valuesSpy.count(), 0);
}

// Reproduces the reported "mode switch sometimes stalls live updates" bug: a
// live retarget (as MainScreen.qml does on Normal<->Favorites switch, without an
// intervening stop()) must not leave the *previous* target set's interval timer
// ticking. If it survives, it fires beginCycle() a second time on top of the
// still-unanswered new-generation request -- with no cancelAll() in between,
// since beginCycle() (unlike setTargets()) does not cancel -- desyncing the
// outstanding-request count from what's actually in flight.
void PollEngineTest::retargetingAfterACycleCompletesDoesNotLeaveTheOldIntervalTimerArmed()
{
    FakeTransport transport;
    ModbusTransactionManager manager(&transport);
    PollEngine engine(&manager);
    engine.setIntervalMs(20);
    engine.setTargets({target(0, 2)});

    engine.start();
    respondTo(transport, 0, 1); // cycle completes, arms a 20ms interval timer

    // Retarget immediately, as a live mode switch would. This issues one fresh
    // request under the new target set.
    engine.setTargets({target(500, 2)});
    const int chunksAfterRetarget = transport.writtenChunks().size();

    // Long enough for the old interval timer to have fired, if it survived.
    QTest::qWait(40);

    QCOMPARE(transport.writtenChunks().size(), chunksAfterRetarget);
}

void PollEngineTest::multipleTargetsAreCoalescedIntoFewerRequests()
{
    FakeTransport transport;
    ModbusTransactionManager manager(&transport);
    PollEngine engine(&manager);
    engine.setIntervalMs(10000);

    // Three nearby targets fit inside the default gap budget, so one request covers
    // all of them.
    engine.setTargets({target(0, 1), target(2, 1), target(4, 1)});
    QCOMPARE(engine.planCount(), 1);
    QSignalSpy valuesSpy(&engine, &PollEngine::targetRegistersUpdated);

    engine.start();
    QCOMPARE(transport.writtenChunks().size(), 1);
    respondTo(transport, 0, 9);

    QVERIFY(valuesSpy.wait(500));
    QCOMPARE(valuesSpy.count(), 3); // every target still got its own update
}

void PollEngineTest::failedRequestDoesNotStallTheCycle()
{
    FakeTransport transport;
    transport.setSupportsPipelining(true);
    ModbusTransactionManager manager(&transport);
    manager.setTimeoutMs(20);
    manager.setRetryCount(0);
    PollEngine engine(&manager);
    engine.setMaxInFlight(1);
    engine.setIntervalMs(10);
    engine.setTargets({target(0, 1), target(1000, 1)});
    QSignalSpy failedSpy(&engine, &PollEngine::pollFailed);

    engine.start();
    // Never answer; the first request times out and the cycle must still advance to
    // the second rather than hanging forever.
    QVERIFY(failedSpy.wait(2000));
    QTRY_VERIFY(transport.writtenChunks().size() >= 2);
    QVERIFY(engine.isRunning());
}

void PollEngineTest::timedOutRequestEmitsTargetFailedForItsTarget()
{
    FakeTransport transport;
    ModbusTransactionManager manager(&transport);
    manager.setTimeoutMs(20);
    manager.setRetryCount(0);
    PollEngine engine(&manager);
    engine.setIntervalMs(10000);
    engine.setTargets({target(0, 1)});
    QSignalSpy targetFailedSpy(&engine, &PollEngine::targetFailed);

    engine.start();
    // Never answer; the timeout must report the one target this plan covers.

    QVERIFY(targetFailedSpy.wait(2000));
    QCOMPARE(targetFailedSpy.count(), 1);
    QCOMPARE(targetFailedSpy.first().at(0).toInt(), 0); // target index
}

void PollEngineTest::decodeFailureEmitsTargetFailedForItsTarget()
{
    FakeTransport transport;
    ModbusTransactionManager manager(&transport);
    PollEngine engine(&manager);
    engine.setIntervalMs(10000);
    engine.setTargets({target(100, 3)});
    QSignalSpy targetFailedSpy(&engine, &PollEngine::targetFailed);

    engine.start();
    respondWithException(transport, 0);

    QCOMPARE(targetFailedSpy.count(), 1);
    QCOMPARE(targetFailedSpy.first().at(0).toInt(), 0);
    QVERIFY(!targetFailedSpy.first().at(1).toString().isEmpty());
}

void PollEngineTest::coalescedFailureEmitsTargetFailedForEveryCoveredTarget()
{
    FakeTransport transport;
    ModbusTransactionManager manager(&transport);
    PollEngine engine(&manager);
    engine.setIntervalMs(10000);

    // Three nearby targets fit inside the default gap budget, coalescing into one
    // request/plan -- a failure on that one request must mark all three targets,
    // not just the first.
    engine.setTargets({target(0, 1), target(2, 1), target(4, 1)});
    QCOMPARE(engine.planCount(), 1);
    QSignalSpy targetFailedSpy(&engine, &PollEngine::targetFailed);

    engine.start();
    respondWithException(transport, 0);

    QCOMPARE(targetFailedSpy.count(), 3);
    QSet<int> failedTargetIndices;
    for (const QList<QVariant> &call : targetFailedSpy)
        failedTargetIndices.insert(call.at(0).toInt());
    QCOMPARE(failedTargetIndices, QSet<int>({0, 1, 2}));
}

QTEST_GUILESS_MAIN(PollEngineTest)
#include "test_poll_engine.moc"
