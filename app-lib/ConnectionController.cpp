#include "ConnectionController.h"

#include "modbus/ModbusPduCodec.h"

namespace ModbusViewer::AppLib {

using namespace ModbusViewer::Core;

ConnectionController::ConnectionController(QObject *parent)
    : QObject(parent)
    , m_transactionManager(&m_tcpTransport)
    , m_pollEngine(&m_transactionManager)
{
    connect(&m_pollEngine, &PollEngine::targetRegistersUpdated, this,
            [this](int targetIndex, int startAddress, QList<quint16> values) {
                if (m_activeFavoritesModel) {
                    m_activeFavoritesModel->applyRegisterUpdate(targetIndex, startAddress, values);
                    return;
                }
                QList<int> intValues;
                intValues.reserve(values.size());
                for (quint16 value : values)
                    intValues.append(int(value));
                emit holdingRegistersRead(startAddress, intValues);
            });
    connect(&m_pollEngine, &PollEngine::pollFailed, this, &ConnectionController::operationFailed);
    connect(&m_pollEngine, &PollEngine::targetFailed, this, [this](int targetIndex, const QString &reason) {
        if (m_activeFavoritesModel) {
            m_activeFavoritesModel->markStale(targetIndex);
            return;
        }
        emit registerReadFailed(reason);
    });

    connect(&m_transactionManager, &ModbusTransactionManager::frameSent, this, [this](const QByteArray &frame) {
        emit communicationLogged(0, QStringLiteral("Tx: ") + QString::fromLatin1(frame.toHex(' ')).toUpper());
    });
    connect(&m_transactionManager, &ModbusTransactionManager::frameReceived, this, [this](const QByteArray &frame) {
        emit communicationLogged(1, QStringLiteral("Rx: ") + QString::fromLatin1(frame.toHex(' ')).toUpper());
    });
    connect(&m_transactionManager, &ModbusTransactionManager::requestFailed, this, [this](quint64, const QString &reason) {
        emit communicationLogged(2, QStringLiteral("Timeout: ") + reason);
    });

    for (ITransport *transport : {static_cast<ITransport *>(&m_tcpTransport),
                                  static_cast<ITransport *>(&m_serialTransport)}) {
        connect(transport, &ITransport::connectionStateChanged, this,
                &ConnectionController::handleTransportConnectionStateChanged);
        connect(transport, &ITransport::errorOccurred, this, &ConnectionController::handleTransportError);
    }
    connect(&m_transactionManager, &ModbusTransactionManager::responseReceived, this,
            &ConnectionController::handleResponseReceived);
    connect(&m_transactionManager, &ModbusTransactionManager::requestFailed, this,
            &ConnectionController::handleRequestFailed);

    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &ConnectionController::beginReconnectAttempt);
}

ConnectionController::~ConnectionController()
{
    // Belt-and-suspenders alongside the transports' own destructors (see their
    // comments): guarantees nothing here reacts once teardown has begun, regardless
    // of member destruction order.
    m_reconnectTimer.stop();
    m_tcpTransport.disconnect(this);
    m_serialTransport.disconnect(this);
}

ITransport *ConnectionController::activeTransport() const
{
    return m_connectionType == ConnectionType::Tcp
        ? static_cast<ITransport *>(const_cast<TcpTransport *>(&m_tcpTransport))
        : static_cast<ITransport *>(const_cast<SerialTransport *>(&m_serialTransport));
}

ConnectionController::ConnectionState ConnectionController::state() const
{
    return m_state;
}

QString ConnectionController::errorMessage() const
{
    return m_errorMessage;
}

bool ConnectionController::canConnect() const
{
    return m_connectionType == ConnectionType::Tcp ? !m_host.isEmpty() : !m_portName.isEmpty();
}

QString ConnectionController::host() const
{
    return m_host;
}

void ConnectionController::setHost(const QString &host)
{
    if (m_host == host)
        return;
    m_host = host;
    emit hostChanged();
    emit canConnectChanged();
}

int ConnectionController::port() const
{
    return m_port;
}

void ConnectionController::setPort(int port)
{
    if (m_port == port)
        return;
    m_port = port;
    emit portChanged();
}

int ConnectionController::unitId() const
{
    return m_unitId;
}

void ConnectionController::setUnitId(int unitId)
{
    if (m_unitId == unitId)
        return;
    m_unitId = unitId;
    emit unitIdChanged();
}

int ConnectionController::timeoutMs() const
{
    return m_timeoutMs;
}

void ConnectionController::setTimeoutMs(int timeoutMs)
{
    if (m_timeoutMs == timeoutMs)
        return;
    m_timeoutMs = timeoutMs;
    emit timeoutMsChanged();
}

int ConnectionController::retryCount() const
{
    return m_retryCount;
}

void ConnectionController::setRetryCount(int retryCount)
{
    if (m_retryCount == retryCount)
        return;
    m_retryCount = retryCount;
    emit retryCountChanged();
}

void ConnectionController::openActiveTransport()
{
    m_transactionManager.setTimeoutMs(m_timeoutMs);
    m_transactionManager.setRetryCount(m_retryCount);
    m_transactionManager.setTransport(activeTransport());

    if (m_connectionType == ConnectionType::Tcp) {
        m_transactionManager.setFramingMode(ModbusTransactionManager::FramingMode::Tcp);
        m_tcpTransport.setHost(m_host);
        m_tcpTransport.setPort(quint16(m_port));
    } else {
        m_transactionManager.setFramingMode(ModbusTransactionManager::FramingMode::Rtu);
        SerialPortSettings settings;
        settings.portName = m_portName;
        settings.baudRate = m_baudRate;
        settings.dataBits = QSerialPort::DataBits(m_dataBits);
        settings.parity = QSerialPort::Parity(m_parity);
        settings.stopBits = QSerialPort::StopBits(m_stopBits);
        m_serialTransport.setSettings(settings);
    }

    activeTransport()->open();
}

void ConnectionController::connectToDevice()
{
    m_resumePollingOnReconnect = false;
    setState(ConnectionState::Connecting);
    openActiveTransport();
}

void ConnectionController::disconnectFromDevice()
{
    // An explicit disconnect ends the session outright: no retrying, and nothing to
    // resume if the device reappears.
    m_reconnectTimer.stop();
    m_resumePollingOnReconnect = false;
    stopPolling();
    // The QML FavoritesModel instance is destroyed when StackView pops MainScreen
    // back to ConnectionScreen on disconnect -- this pointer must not outlive it.
    m_activeFavoritesModel = nullptr;
    activeTransport()->close();
    setState(ConnectionState::Disconnected);
}

void ConnectionController::beginReconnectAttempt()
{
    if (m_state != ConnectionState::ConnectionLost)
        return; // the user disconnected, or we are already back

    openActiveTransport();
}

void ConnectionController::startPolling(int startAddress, int quantity)
{
    m_pollStartAddress = startAddress;
    m_pollQuantity = quantity;

    PollTarget target;
    target.unitId = quint8(m_unitId);
    target.registerType = RegisterType::HoldingRegister;
    target.startAddress = quint16(startAddress);
    target.quantity = quint16(quantity);

    beginPolling({target}, nullptr);
}

void ConnectionController::startPollingFavorites(FavoritesModel *favoritesModel)
{
    if (!favoritesModel)
        return;
    beginPolling(favoritesModel->buildPollTargets(quint8(m_unitId)), favoritesModel);
}

void ConnectionController::beginPolling(const QList<PollTarget> &targets, FavoritesModel *favoritesModel)
{
    m_activeFavoritesModel = favoritesModel;
    m_pollEngine.setTargets(targets);
    m_pollEngine.setIntervalMs(m_pollIntervalMs);
    m_pollEngine.start();
    emit pollingChanged();
}

void ConnectionController::stopPolling()
{
    if (!m_pollEngine.isRunning())
        return;
    m_pollEngine.stop();
    emit pollingChanged();
}

bool ConnectionController::isPolling() const
{
    return m_pollEngine.isRunning();
}

int ConnectionController::pollIntervalMs() const
{
    return m_pollIntervalMs;
}

void ConnectionController::setPollIntervalMs(int intervalMs)
{
    if (m_pollIntervalMs == intervalMs)
        return;
    m_pollIntervalMs = intervalMs;
    m_pollEngine.setIntervalMs(intervalMs); // applies live if a poll loop is running
    emit pollIntervalMsChanged();
}

int ConnectionController::reconnectIntervalMs() const
{
    return m_reconnectIntervalMs;
}

void ConnectionController::setReconnectIntervalMs(int intervalMs)
{
    if (m_reconnectIntervalMs == intervalMs)
        return;
    m_reconnectIntervalMs = intervalMs;
    emit reconnectIntervalMsChanged();
}

ConnectionController::ConnectionType ConnectionController::connectionType() const
{
    return m_connectionType;
}

void ConnectionController::setConnectionType(ConnectionType type)
{
    if (m_connectionType == type)
        return;
    m_connectionType = type;
    emit connectionTypeChanged();
    emit canConnectChanged();
}

QString ConnectionController::portName() const
{
    return m_portName;
}

void ConnectionController::setPortName(const QString &portName)
{
    if (m_portName == portName)
        return;
    m_portName = portName;
    emit portNameChanged();
    emit canConnectChanged();
}

int ConnectionController::baudRate() const
{
    return m_baudRate;
}

void ConnectionController::setBaudRate(int baudRate)
{
    if (m_baudRate == baudRate)
        return;
    m_baudRate = baudRate;
    emit baudRateChanged();
}

int ConnectionController::dataBits() const
{
    return m_dataBits;
}

void ConnectionController::setDataBits(int dataBits)
{
    if (m_dataBits == dataBits)
        return;
    m_dataBits = dataBits;
    emit dataBitsChanged();
}

int ConnectionController::parity() const
{
    return m_parity;
}

void ConnectionController::setParity(int parity)
{
    if (m_parity == parity)
        return;
    m_parity = parity;
    emit parityChanged();
}

int ConnectionController::stopBits() const
{
    return m_stopBits;
}

void ConnectionController::setStopBits(int stopBits)
{
    if (m_stopBits == stopBits)
        return;
    m_stopBits = stopBits;
    emit stopBitsChanged();
}

void ConnectionController::readHoldingRegisters(int startAddress, int quantity)
{
    m_pendingOperation = PendingOperation::ReadHoldingRegisters;
    m_pendingStartAddress = startAddress;
    const QByteArray pdu =
        encodeReadRequest(FunctionCode::ReadHoldingRegisters, quint16(startAddress), quint16(quantity));
    m_transactionManager.sendRequest(quint8(m_unitId), pdu);
}

void ConnectionController::writeSingleRegister(int address, int value)
{
    m_pendingOperation = PendingOperation::WriteSingleRegister;
    m_pendingStartAddress = address;
    const QByteArray pdu = encodeWriteSingleRegisterRequest(quint16(address), quint16(value));
    m_transactionManager.sendRequest(quint8(m_unitId), pdu);
}

void ConnectionController::setState(ConnectionState newState)
{
    if (m_state == newState)
        return;
    m_state = newState;
    emit stateChanged();
}

void ConnectionController::handleTransportConnectionStateChanged(bool connected)
{
    if (connected) {
        m_reconnectTimer.stop();
        setState(ConnectionState::Connected);

        // Coming back from a loss: pick the poll loop up where it left off, in
        // whichever mode was active -- m_activeFavoritesModel survived the pause
        // (stopPolling() below does not clear it).
        if (m_resumePollingOnReconnect) {
            m_resumePollingOnReconnect = false;
            if (m_activeFavoritesModel)
                startPollingFavorites(m_activeFavoritesModel);
            else
                startPolling(m_pollStartAddress, m_pollQuantity);
        }
        return;
    }

    // Losing an established connection is not the same as the user ending one. Hold
    // the last values on screen, stop asking the device for more, and keep retrying.
    if (m_state == ConnectionState::Connected || m_state == ConnectionState::ConnectionLost) {
        if (isPolling()) {
            m_resumePollingOnReconnect = true;
            stopPolling();
        }
        setState(ConnectionState::ConnectionLost);
        m_reconnectTimer.start(m_reconnectIntervalMs);
    }
}

void ConnectionController::handleTransportError(const QString &message)
{
    emit communicationLogged(2, QStringLiteral("Error: ") + message);

    m_errorMessage = message;
    emit errorMessageChanged();

    // An error on a connection that was already up is a loss, not a failure to
    // connect - a remote close typically arrives as an error rather than a clean
    // disconnect. Failed is reserved for never having got established.
    if (m_state == ConnectionState::Connected) {
        handleTransportConnectionStateChanged(false);
        return;
    }

    // While reconnecting, a refused connection is an expected step in the retry
    // loop, not a new failure - stay in ConnectionLost and try again later.
    if (m_state == ConnectionState::ConnectionLost) {
        m_reconnectTimer.start(m_reconnectIntervalMs);
        return;
    }

    setState(ConnectionState::Failed);
}

void ConnectionController::handleResponseReceived(quint64, const QByteArray &responsePdu)
{
    // PollEngine tags its own requests and handles them separately; anything landing
    // here is a one-shot read/write issued by this controller.
    switch (m_pendingOperation) {
    case PendingOperation::ReadHoldingRegisters: {
        const auto result = decodeReadHoldingRegistersResponse(responsePdu);
        if (!result.ok()) {
            emit operationFailed(result.errorMessage);
            break;
        }
        QList<int> values;
        for (quint16 value : result.value.values)
            values.append(int(value));
        emit holdingRegistersRead(m_pendingStartAddress, values);
        break;
    }
    case PendingOperation::WriteSingleRegister: {
        const auto result = decodeWriteSingleRegisterResponse(responsePdu);
        if (!result.ok()) {
            emit operationFailed(result.errorMessage);
            break;
        }
        emit singleRegisterWritten(int(result.value.address), int(result.value.value));
        break;
    }
    case PendingOperation::None:
        break;
    }
    m_pendingOperation = PendingOperation::None;
}

void ConnectionController::handleRequestFailed(quint64, const QString &reason)
{
    m_pendingOperation = PendingOperation::None;
    emit operationFailed(reason);
}

} // namespace ModbusViewer::AppLib
