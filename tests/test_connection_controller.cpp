#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

#include "ConnectionController.h"
#include "models/FavoritesModel.h"

using ModbusViewer::AppLib::ConnectionController;
using ModbusViewer::AppLib::FavoritesModel;
using ModbusViewer::Core::RegisterType;

namespace {

// A minimal Modbus TCP slave. Real socket, so closing it produces a genuine
// transport loss rather than a simulated one - which is exactly what M5c is about.
class FakeModbusServer : public QObject
{
    Q_OBJECT

public:
    bool listen(quint16 preferredPort = 0)
    {
        m_server = new QTcpServer(this);
        connect(m_server, &QTcpServer::newConnection, this, &FakeModbusServer::acceptConnection);
        if (!m_server->listen(QHostAddress::LocalHost, preferredPort))
            return false;
        m_port = m_server->serverPort();
        return true;
    }

    quint16 port() const { return m_port; }

    // Drops the listening socket and every client - the app should see this as a
    // hard transport loss.
    void shutdown()
    {
        for (QTcpSocket *client : m_clients) {
            client->abort();
            client->deleteLater();
        }
        m_clients.clear();
        if (m_server) {
            m_server->close();
            m_server->deleteLater();
            m_server = nullptr;
        }
    }

    int requestsServed() const { return m_requestsServed; }

    // When true, FC01 (Read Coils) requests are received but never answered --
    // simulates a poll target reliably timing out, for testing that
    // ConnectionController's one-shot request/response handling stays isolated
    // from unrelated PollEngine traffic sharing the same transaction manager.
    void setBlackholeReadCoils(bool blackhole) { m_blackholeReadCoils = blackhole; }

private slots:
    void acceptConnection()
    {
        while (m_server && m_server->hasPendingConnections()) {
            QTcpSocket *client = m_server->nextPendingConnection();
            m_clients.append(client);
            connect(client, &QTcpSocket::readyRead, this, [this, client] { respond(client); });
        }
    }

private:
    // Answers any read-registers request with the requested number of registers, all
    // zero; a read-bits request with every requested bit set (0xFF-packed); and a
    // write-single-coil request by echoing the request PDU, per spec.
    void respond(QTcpSocket *client)
    {
        QByteArray buffer = client->readAll();
        while (buffer.size() >= 12) {
            const QByteArray request = buffer.left(12);
            buffer = buffer.mid(12);

            const quint16 transactionId = (quint8(request[0]) << 8) | quint8(request[1]);
            const quint8 unitId = quint8(request[6]);
            const quint8 functionCode = quint8(request[7]);

            if (functionCode == 0x01 && m_blackholeReadCoils) {
                // Drop it entirely: no response written, no ++m_requestsServed --
                // the caller times out waiting, same as a genuinely unresponsive
                // slave for that one function code.
                continue;
            }

            QByteArray pdu;
            if (functionCode == 0x01 || functionCode == 0x02) {
                const int quantity = (quint8(request[10]) << 8) | quint8(request[11]);
                const int byteCount = (quantity + 7) / 8;
                pdu.append(char(functionCode));
                pdu.append(char(quint8(byteCount)));
                pdu.append(QByteArray(byteCount, char(0xFF)));
            } else if (functionCode == 0x05) {
                pdu = request.mid(7);
            } else {
                const int quantity = (quint8(request[10]) << 8) | quint8(request[11]);
                pdu.append(char(functionCode));
                pdu.append(char(quint8(quantity * 2)));
                pdu.append(QByteArray(quantity * 2, char(0)));
            }

            QByteArray frame;
            frame.append(char((transactionId >> 8) & 0xFF));
            frame.append(char(transactionId & 0xFF));
            frame.append(char(0));
            frame.append(char(0));
            const int length = 1 + pdu.size();
            frame.append(char((length >> 8) & 0xFF));
            frame.append(char(length & 0xFF));
            frame.append(char(unitId));
            frame.append(pdu);

            client->write(frame);
            ++m_requestsServed;
        }
    }

    QTcpServer *m_server = nullptr;
    QList<QTcpSocket *> m_clients;
    quint16 m_port = 0;
    int m_requestsServed = 0;
    bool m_blackholeReadCoils = false;
};

bool waitForState(const ConnectionController &controller, ConnectionController::ConnectionState state,
                  int timeoutMs = 5000)
{
    QElapsedTimer elapsed;
    elapsed.start();
    while (controller.state() != state && elapsed.elapsed() < timeoutMs)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return controller.state() == state;
}

} // namespace

class ConnectionControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void connectsToAReachableServer();
    void hardTransportLossReportsConnectionLostRatherThanDisconnected();
    void userInitiatedDisconnectReportsDisconnectedNotConnectionLost();
    void connectionLossStopsPollingButKeepsTheLastValues();
    void autoReconnectRestoresTheConnectionAndResumesPolling();
    void hardTransportLossEmitsConnectionLostSignalExactlyOnce();
    void autoReconnectEmitsConnectionRestoredSignalExactlyOnce();
    void freshInitialConnectDoesNotEmitConnectionRestored();
    void userDisconnectDuringConnectionLossStopsReconnectAttempts();
    void switchingToFavoritesModeStopsRoutingUpdatesToNormalSignal();
    void autoReconnectResumesFavoritesModeIfThatWasActive();
    void connectionLossDuringPollingEmitsErrorCommunicationLogEntry();

    void readRegistersForCoilsEmitsBitsRead();
    void writeSingleCoilRoundTripsAndEmitsSingleCoilWritten();
    void startPollingWithDiscreteInputRegisterTypeRoutesBitUpdatesIntoFavoritesModel();
    void autoReconnectResumesPollingWithTheSameRegisterTypeThatWasActive();

    void reRequestingFavoritesPollingAfterAddingAnEntryPicksUpTheNewTarget();
    void unrelatedPollTimeoutDuringAOneShotCoilWriteDoesNotCorruptTheWriteResponse();
};

void ConnectionControllerTest::connectsToAReachableServer()
{
    FakeModbusServer server;
    QVERIFY(server.listen());

    ConnectionController controller;
    controller.setConnectionType(ConnectionController::ConnectionType::Tcp);
    controller.setHost(QStringLiteral("127.0.0.1"));
    controller.setPort(server.port());
    controller.connectToDevice();

    QVERIFY(waitForState(controller, ConnectionController::ConnectionState::Connected));
}

void ConnectionControllerTest::hardTransportLossReportsConnectionLostRatherThanDisconnected()
{
    FakeModbusServer server;
    QVERIFY(server.listen());

    ConnectionController controller;
    controller.setConnectionType(ConnectionController::ConnectionType::Tcp);
    controller.setHost(QStringLiteral("127.0.0.1"));
    controller.setPort(server.port());
    controller.setReconnectIntervalMs(10000); // keep reconnect out of this test
    controller.connectToDevice();
    QVERIFY(waitForState(controller, ConnectionController::ConnectionState::Connected));

    server.shutdown();

    // The distinction matters: ConnectionLost keeps the user on the data screen with
    // their last values, Disconnected sends them back to the connection form.
    QVERIFY(waitForState(controller, ConnectionController::ConnectionState::ConnectionLost));
}

void ConnectionControllerTest::userInitiatedDisconnectReportsDisconnectedNotConnectionLost()
{
    FakeModbusServer server;
    QVERIFY(server.listen());

    ConnectionController controller;
    controller.setConnectionType(ConnectionController::ConnectionType::Tcp);
    controller.setHost(QStringLiteral("127.0.0.1"));
    controller.setPort(server.port());
    controller.connectToDevice();
    QVERIFY(waitForState(controller, ConnectionController::ConnectionState::Connected));

    controller.disconnectFromDevice();

    QCOMPARE(controller.state(), ConnectionController::ConnectionState::Disconnected);
    QVERIFY(!controller.isPolling());
}

void ConnectionControllerTest::connectionLossStopsPollingButKeepsTheLastValues()
{
    FakeModbusServer server;
    QVERIFY(server.listen());

    ConnectionController controller;
    controller.setConnectionType(ConnectionController::ConnectionType::Tcp);
    controller.setHost(QStringLiteral("127.0.0.1"));
    controller.setPort(server.port());
    controller.setReconnectIntervalMs(10000);
    controller.setPollIntervalMs(20);
    controller.connectToDevice();
    QVERIFY(waitForState(controller, ConnectionController::ConnectionState::Connected));

    QSignalSpy valuesSpy(&controller, &ConnectionController::holdingRegistersRead);
    controller.startPolling(int(RegisterType::HoldingRegister), 0, 4);
    QVERIFY(valuesSpy.wait(3000));
    const int valuesBeforeLoss = valuesSpy.count();

    server.shutdown();
    QVERIFY(waitForState(controller, ConnectionController::ConnectionState::ConnectionLost));

    // No further reads can succeed with the device gone, and crucially the app does
    // not clear what it already has - the view keeps showing the last known values
    // under a watermark.
    QVERIFY(!controller.isPolling());
    QTest::qWait(200);
    QCOMPARE(valuesSpy.count(), valuesBeforeLoss);
}

void ConnectionControllerTest::autoReconnectRestoresTheConnectionAndResumesPolling()
{
    FakeModbusServer server;
    QVERIFY(server.listen());
    const quint16 port = server.port();

    ConnectionController controller;
    controller.setConnectionType(ConnectionController::ConnectionType::Tcp);
    controller.setHost(QStringLiteral("127.0.0.1"));
    controller.setPort(port);
    controller.setPollIntervalMs(20);
    controller.setReconnectIntervalMs(100);
    controller.connectToDevice();
    QVERIFY(waitForState(controller, ConnectionController::ConnectionState::Connected));

    controller.startPolling(int(RegisterType::HoldingRegister), 0, 4);
    QSignalSpy valuesSpy(&controller, &ConnectionController::holdingRegistersRead);
    QVERIFY(valuesSpy.wait(3000));

    server.shutdown();
    QVERIFY(waitForState(controller, ConnectionController::ConnectionState::ConnectionLost));

    // Bring the device back on the same port; the app should notice on its own.
    FakeModbusServer restarted;
    QVERIFY(restarted.listen(port));

    QVERIFY(waitForState(controller, ConnectionController::ConnectionState::Connected, 8000));

    // Polling resumes against the same target set without the user reconfiguring it.
    QVERIFY(controller.isPolling());
    QSignalSpy resumedSpy(&controller, &ConnectionController::holdingRegistersRead);
    QVERIFY(resumedSpy.wait(3000));
}

// connectionLost/connectionRestored exist so SessionLogger (M12a) can count
// disconnects/reconnects without inferring them from generic stateChanged().
void ConnectionControllerTest::hardTransportLossEmitsConnectionLostSignalExactlyOnce()
{
    FakeModbusServer server;
    QVERIFY(server.listen());

    ConnectionController controller;
    controller.setConnectionType(ConnectionController::ConnectionType::Tcp);
    controller.setHost(QStringLiteral("127.0.0.1"));
    controller.setPort(server.port());
    controller.setReconnectIntervalMs(10000); // keep reconnect out of this test
    controller.connectToDevice();
    QVERIFY(waitForState(controller, ConnectionController::ConnectionState::Connected));

    QSignalSpy lostSpy(&controller, &ConnectionController::connectionLost);

    server.shutdown();
    QVERIFY(waitForState(controller, ConnectionController::ConnectionState::ConnectionLost));

    QCOMPARE(lostSpy.count(), 1);
}

void ConnectionControllerTest::autoReconnectEmitsConnectionRestoredSignalExactlyOnce()
{
    FakeModbusServer server;
    QVERIFY(server.listen());
    const quint16 port = server.port();

    ConnectionController controller;
    controller.setConnectionType(ConnectionController::ConnectionType::Tcp);
    controller.setHost(QStringLiteral("127.0.0.1"));
    controller.setPort(port);
    controller.setReconnectIntervalMs(100);
    controller.connectToDevice();
    QVERIFY(waitForState(controller, ConnectionController::ConnectionState::Connected));

    server.shutdown();
    QVERIFY(waitForState(controller, ConnectionController::ConnectionState::ConnectionLost));

    QSignalSpy restoredSpy(&controller, &ConnectionController::connectionRestored);

    FakeModbusServer restarted;
    QVERIFY(restarted.listen(port));
    QVERIFY(waitForState(controller, ConnectionController::ConnectionState::Connected, 8000));

    QCOMPARE(restoredSpy.count(), 1);
}

// The exact bug the self-review catch (m_resumePollingOnReconnect being a false
// proxy for "was this a reconnect") would have caused: a fresh initial connect
// must not be mistaken for a recovery.
void ConnectionControllerTest::freshInitialConnectDoesNotEmitConnectionRestored()
{
    FakeModbusServer server;
    QVERIFY(server.listen());

    ConnectionController controller;
    controller.setConnectionType(ConnectionController::ConnectionType::Tcp);
    controller.setHost(QStringLiteral("127.0.0.1"));
    controller.setPort(server.port());

    QSignalSpy restoredSpy(&controller, &ConnectionController::connectionRestored);
    controller.connectToDevice();
    QVERIFY(waitForState(controller, ConnectionController::ConnectionState::Connected));

    QCOMPARE(restoredSpy.count(), 0);
}

void ConnectionControllerTest::userDisconnectDuringConnectionLossStopsReconnectAttempts()
{
    FakeModbusServer server;
    QVERIFY(server.listen());
    const quint16 port = server.port();

    ConnectionController controller;
    controller.setConnectionType(ConnectionController::ConnectionType::Tcp);
    controller.setHost(QStringLiteral("127.0.0.1"));
    controller.setPort(port);
    controller.setReconnectIntervalMs(50);
    controller.connectToDevice();
    QVERIFY(waitForState(controller, ConnectionController::ConnectionState::Connected));

    server.shutdown();
    QVERIFY(waitForState(controller, ConnectionController::ConnectionState::ConnectionLost));

    controller.disconnectFromDevice();
    QCOMPARE(controller.state(), ConnectionController::ConnectionState::Disconnected);

    // Giving up is the user's decision: bringing the device back must not silently
    // reconnect them.
    FakeModbusServer restarted;
    QVERIFY(restarted.listen(port));
    QTest::qWait(300);
    QCOMPARE(controller.state(), ConnectionController::ConnectionState::Disconnected);
}

void ConnectionControllerTest::switchingToFavoritesModeStopsRoutingUpdatesToNormalSignal()
{
    FakeModbusServer server;
    QVERIFY(server.listen());

    ConnectionController controller;
    controller.setConnectionType(ConnectionController::ConnectionType::Tcp);
    controller.setHost(QStringLiteral("127.0.0.1"));
    controller.setPort(server.port());
    controller.setPollIntervalMs(20);
    controller.connectToDevice();
    QVERIFY(waitForState(controller, ConnectionController::ConnectionState::Connected));

    QSignalSpy valuesSpy(&controller, &ConnectionController::holdingRegistersRead);
    controller.startPolling(int(RegisterType::HoldingRegister), 0, 4);
    QVERIFY(valuesSpy.wait(3000));

    FavoritesModel favorites;
    favorites.addAdHoc(int(RegisterType::HoldingRegister), 50);
    QSignalSpy favoritesSpy(&favorites, &QAbstractItemModel::dataChanged);
    controller.startPollingFavorites(&favorites);

    // Any Normal-mode responses still in flight at the moment of the switch belong
    // to an abandoned generation and must be dropped (PollEngine::setTargets), not
    // delivered here -- proving the mode swap actually reaches this integration
    // surface, not just PollEngine in isolation.
    const int normalCountAtSwitch = valuesSpy.count();
    QVERIFY(favoritesSpy.wait(3000));
    QTest::qWait(200);
    QCOMPARE(valuesSpy.count(), normalCountAtSwitch);
}

void ConnectionControllerTest::autoReconnectResumesFavoritesModeIfThatWasActive()
{
    FakeModbusServer server;
    QVERIFY(server.listen());
    const quint16 port = server.port();

    ConnectionController controller;
    controller.setConnectionType(ConnectionController::ConnectionType::Tcp);
    controller.setHost(QStringLiteral("127.0.0.1"));
    controller.setPort(port);
    controller.setPollIntervalMs(20);
    controller.setReconnectIntervalMs(100);
    controller.connectToDevice();
    QVERIFY(waitForState(controller, ConnectionController::ConnectionState::Connected));

    FavoritesModel favorites;
    favorites.addAdHoc(int(RegisterType::HoldingRegister), 5);
    controller.startPollingFavorites(&favorites);
    QSignalSpy favoritesSpy(&favorites, &QAbstractItemModel::dataChanged);
    QVERIFY(favoritesSpy.wait(3000));

    server.shutdown();
    QVERIFY(waitForState(controller, ConnectionController::ConnectionState::ConnectionLost));

    FakeModbusServer restarted;
    QVERIFY(restarted.listen(port));
    QVERIFY(waitForState(controller, ConnectionController::ConnectionState::Connected, 8000));

    QVERIFY(controller.isPolling());
    QSignalSpy resumedFavoritesSpy(&favorites, &QAbstractItemModel::dataChanged);
    QVERIFY(resumedFavoritesSpy.wait(3000));

    // Resumed into Favorites, not Normal -- the Normal-mode signal must stay silent.
    QSignalSpy normalSpy(&controller, &ConnectionController::holdingRegistersRead);
    QTest::qWait(100);
    QCOMPARE(normalSpy.count(), 0);
}

void ConnectionControllerTest::connectionLossDuringPollingEmitsErrorCommunicationLogEntry()
{
    FakeModbusServer server;
    QVERIFY(server.listen());

    ConnectionController controller;
    controller.setConnectionType(ConnectionController::ConnectionType::Tcp);
    controller.setHost(QStringLiteral("127.0.0.1"));
    controller.setPort(server.port());
    controller.setPollIntervalMs(20);
    controller.connectToDevice();
    QVERIFY(waitForState(controller, ConnectionController::ConnectionState::Connected));

    controller.startPolling(int(RegisterType::HoldingRegister), 0, 4);
    QSignalSpy logSpy(&controller, &ConnectionController::communicationLogged);

    server.shutdown();
    QVERIFY(waitForState(controller, ConnectionController::ConnectionState::ConnectionLost));

    // direction == 2 is CommunicationLogModel::Direction::Error.
    bool sawErrorEntry = false;
    for (const QList<QVariant> &call : logSpy)
        sawErrorEntry = sawErrorEntry || call.at(0).toInt() == 2;
    QVERIFY(sawErrorEntry);
}

void ConnectionControllerTest::readRegistersForCoilsEmitsBitsRead()
{
    FakeModbusServer server;
    QVERIFY(server.listen());

    ConnectionController controller;
    controller.setConnectionType(ConnectionController::ConnectionType::Tcp);
    controller.setHost(QStringLiteral("127.0.0.1"));
    controller.setPort(server.port());
    controller.connectToDevice();
    QVERIFY(waitForState(controller, ConnectionController::ConnectionState::Connected));

    QSignalSpy bitsSpy(&controller, &ConnectionController::bitsRead);
    controller.readRegisters(int(RegisterType::Coil), 0, 3);

    QVERIFY(bitsSpy.wait(3000));
    QCOMPARE(bitsSpy.first().at(0).toInt(), 0);
    QCOMPARE(bitsSpy.first().at(1).value<QList<bool>>(), (QList<bool>{true, true, true}));
}

void ConnectionControllerTest::writeSingleCoilRoundTripsAndEmitsSingleCoilWritten()
{
    FakeModbusServer server;
    QVERIFY(server.listen());

    ConnectionController controller;
    controller.setConnectionType(ConnectionController::ConnectionType::Tcp);
    controller.setHost(QStringLiteral("127.0.0.1"));
    controller.setPort(server.port());
    controller.connectToDevice();
    QVERIFY(waitForState(controller, ConnectionController::ConnectionState::Connected));

    QSignalSpy coilWrittenSpy(&controller, &ConnectionController::singleCoilWritten);
    controller.writeSingleCoil(5, true);

    QVERIFY(coilWrittenSpy.wait(3000));
    QCOMPARE(coilWrittenSpy.first().at(0).toInt(), 5);
    QCOMPARE(coilWrittenSpy.first().at(1).toBool(), true);
}

void ConnectionControllerTest::startPollingWithDiscreteInputRegisterTypeRoutesBitUpdatesIntoFavoritesModel()
{
    FakeModbusServer server;
    QVERIFY(server.listen());

    ConnectionController controller;
    controller.setConnectionType(ConnectionController::ConnectionType::Tcp);
    controller.setHost(QStringLiteral("127.0.0.1"));
    controller.setPort(server.port());
    controller.setPollIntervalMs(20);
    controller.connectToDevice();
    QVERIFY(waitForState(controller, ConnectionController::ConnectionState::Connected));

    FavoritesModel favorites;
    favorites.addAdHoc(int(RegisterType::DiscreteInput), 5);
    QSignalSpy favoritesSpy(&favorites, &QAbstractItemModel::dataChanged);
    controller.startPollingFavorites(&favorites);

    QVERIFY(favoritesSpy.wait(3000));
    QVERIFY(favorites.data(favorites.index(0, 0), FavoritesModel::BoolValueRole).toBool());
}

void ConnectionControllerTest::autoReconnectResumesPollingWithTheSameRegisterTypeThatWasActive()
{
    FakeModbusServer server;
    QVERIFY(server.listen());
    const quint16 port = server.port();

    ConnectionController controller;
    controller.setConnectionType(ConnectionController::ConnectionType::Tcp);
    controller.setHost(QStringLiteral("127.0.0.1"));
    controller.setPort(port);
    controller.setPollIntervalMs(20);
    controller.setReconnectIntervalMs(100);
    controller.connectToDevice();
    QVERIFY(waitForState(controller, ConnectionController::ConnectionState::Connected));

    QSignalSpy bitsSpy(&controller, &ConnectionController::bitsRead);
    controller.startPolling(int(RegisterType::Coil), 0, 2);
    QVERIFY(bitsSpy.wait(3000));

    server.shutdown();
    QVERIFY(waitForState(controller, ConnectionController::ConnectionState::ConnectionLost));

    FakeModbusServer restarted;
    QVERIFY(restarted.listen(port));
    QVERIFY(waitForState(controller, ConnectionController::ConnectionState::Connected, 8000));

    QVERIFY(controller.isPolling());
    QSignalSpy resumedBitsSpy(&controller, &ConnectionController::bitsRead);
    QVERIFY(resumedBitsSpy.wait(3000));

    // Resumed as Coil, not HoldingRegister -- the word-register signal must stay
    // silent, proving m_pollRegisterType (not just address/quantity) survived the
    // reconnect.
    QSignalSpy wordsSpy(&controller, &ConnectionController::holdingRegistersRead);
    QTest::qWait(100);
    QCOMPARE(wordsSpy.count(), 0);
}

// Regression test for the known "add/remove while Favorites-mode polling is
// active desyncs targetIndex until restarted" gap (flagged since M6c):
// MainScreen.qml now re-calls startPollingFavorites() after every add/remove
// while polling, and this proves that re-call actually picks up new entries
// rather than being a no-op.
void ConnectionControllerTest::reRequestingFavoritesPollingAfterAddingAnEntryPicksUpTheNewTarget()
{
    FakeModbusServer server;
    QVERIFY(server.listen());

    ConnectionController controller;
    controller.setConnectionType(ConnectionController::ConnectionType::Tcp);
    controller.setHost(QStringLiteral("127.0.0.1"));
    controller.setPort(server.port());
    controller.setPollIntervalMs(20);
    controller.connectToDevice();
    QVERIFY(waitForState(controller, ConnectionController::ConnectionState::Connected));

    FavoritesModel favorites;
    // Empty when polling starts -- buildPollTargets() returns zero targets, so
    // nothing is polled yet. This is the "empty list, not a bug" case: polling
    // correctly does nothing because there is nothing to poll.
    controller.startPollingFavorites(&favorites);
    QVERIFY(controller.isPolling());

    favorites.addAdHoc(int(RegisterType::HoldingRegister), 10);
    QSignalSpy favoritesSpy(&favorites, &QAbstractItemModel::dataChanged);

    // Re-requesting favorites polling after the add -- what
    // retargetFavoritesPollingIfActive() does in MainScreen.qml -- must pick up
    // the newly added entry without the user needing to stop/restart manually.
    controller.startPollingFavorites(&favorites);

    QVERIFY(favoritesSpy.wait(3000));
}

// Regression test for a bug found via a real user report: ConnectionController
// and PollEngine share one ModbusTransactionManager, and both connect to its
// responseReceived/requestFailed signals -- so a poll target's own timeout was
// firing ConnectionController::handleRequestFailed too, which (with no
// correlation-id check) blindly reset m_pendingOperation and emitted
// operationFailed as if *its own* one-shot request had failed, even though the
// failure actually belonged to an unrelated poll target. In practice: toggling a
// Favorites Coil while another Favorites entry's poll target was timing out
// left the coil write's own response silently dropped (m_pendingOperation had
// already been reset to None by the unrelated poll timeout) and surfaced a
// spurious "request timed out" that had nothing to do with the write.
void ConnectionControllerTest::unrelatedPollTimeoutDuringAOneShotCoilWriteDoesNotCorruptTheWriteResponse()
{
    FakeModbusServer server;
    QVERIFY(server.listen());

    ConnectionController controller;
    controller.setConnectionType(ConnectionController::ConnectionType::Tcp);
    controller.setHost(QStringLiteral("127.0.0.1"));
    controller.setPort(server.port());
    controller.setTimeoutMs(50);
    controller.setRetryCount(0);
    controller.setPollIntervalMs(20);
    controller.connectToDevice();
    QVERIFY(waitForState(controller, ConnectionController::ConnectionState::Connected));

    FavoritesModel favorites;
    favorites.addAdHoc(int(RegisterType::Coil), 5);
    favorites.addAdHoc(int(RegisterType::HoldingRegister), 10);
    controller.startPollingFavorites(&favorites);

    // The Coil entry's poll target will now time out on every cycle, repeatedly
    // firing ModbusTransactionManager::requestFailed while the one-shot write
    // below is also outstanding.
    server.setBlackholeReadCoils(true);

    QSignalSpy operationFailedSpy(&controller, &ConnectionController::operationFailed);
    QSignalSpy coilWrittenSpy(&controller, &ConnectionController::singleCoilWritten);

    controller.writeSingleCoil(5, true);

    QVERIFY(coilWrittenSpy.wait(3000));
    QCOMPARE(coilWrittenSpy.first().at(0).toInt(), 5);
    QCOMPARE(coilWrittenSpy.first().at(1).toBool(), true);
    // The unrelated, repeatedly-timing-out poll target must not have been
    // misattributed as this one-shot write failing.
    QCOMPARE(operationFailedSpy.count(), 0);
}

QTEST_MAIN(ConnectionControllerTest)
#include "test_connection_controller.moc"
