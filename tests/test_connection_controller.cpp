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
    // Answers any read request with the requested number of registers, all zero.
    void respond(QTcpSocket *client)
    {
        QByteArray buffer = client->readAll();
        while (buffer.size() >= 12) {
            const QByteArray request = buffer.left(12);
            buffer = buffer.mid(12);

            const quint16 transactionId = (quint8(request[0]) << 8) | quint8(request[1]);
            const quint8 unitId = quint8(request[6]);
            const quint8 functionCode = quint8(request[7]);
            const int quantity = (quint8(request[10]) << 8) | quint8(request[11]);

            QByteArray pdu;
            pdu.append(char(functionCode));
            pdu.append(char(quint8(quantity * 2)));
            pdu.append(QByteArray(quantity * 2, char(0)));

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
    void userDisconnectDuringConnectionLossStopsReconnectAttempts();
    void switchingToFavoritesModeStopsRoutingUpdatesToNormalSignal();
    void autoReconnectResumesFavoritesModeIfThatWasActive();
    void connectionLossDuringPollingEmitsErrorCommunicationLogEntry();
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
    controller.startPolling(0, 4);
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

    controller.startPolling(0, 4);
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
    controller.startPolling(0, 4);
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

    controller.startPolling(0, 4);
    QSignalSpy logSpy(&controller, &ConnectionController::communicationLogged);

    server.shutdown();
    QVERIFY(waitForState(controller, ConnectionController::ConnectionState::ConnectionLost));

    // direction == 2 is CommunicationLogModel::Direction::Error.
    bool sawErrorEntry = false;
    for (const QList<QVariant> &call : logSpy)
        sawErrorEntry = sawErrorEntry || call.at(0).toInt() == 2;
    QVERIFY(sawErrorEntry);
}

QTEST_MAIN(ConnectionControllerTest)
#include "test_connection_controller.moc"
