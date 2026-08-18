#pragma once

#include <QList>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QTimer>

#include "modbus/ModbusTransactionManager.h"
#include "models/FavoritesModel.h"
#include "poll/PollEngine.h"
#include "transport/SerialTransport.h"
#include "transport/TcpTransport.h"

namespace ModbusViewer::AppLib {

// v1 is single-connection (Decision 6 baseline): this is the one connection's
// lifecycle and read/write surface, exposed to QML as a singleton so every screen
// binds to the same live state.
class ConnectionController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(ConnectionState state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(bool canConnect READ canConnect NOTIFY canConnectChanged)

    Q_PROPERTY(ConnectionType connectionType READ connectionType WRITE setConnectionType NOTIFY
                   connectionTypeChanged)

    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY hostChanged)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY portChanged)
    Q_PROPERTY(int unitId READ unitId WRITE setUnitId NOTIFY unitIdChanged)
    Q_PROPERTY(int timeoutMs READ timeoutMs WRITE setTimeoutMs NOTIFY timeoutMsChanged)
    Q_PROPERTY(int retryCount READ retryCount WRITE setRetryCount NOTIFY retryCountChanged)

    Q_PROPERTY(bool polling READ isPolling NOTIFY pollingChanged)
    Q_PROPERTY(int pollIntervalMs READ pollIntervalMs WRITE setPollIntervalMs NOTIFY pollIntervalMsChanged)
    Q_PROPERTY(int reconnectIntervalMs READ reconnectIntervalMs WRITE setReconnectIntervalMs NOTIFY
                   reconnectIntervalMsChanged)

    Q_PROPERTY(QString portName READ portName WRITE setPortName NOTIFY portNameChanged)
    Q_PROPERTY(int baudRate READ baudRate WRITE setBaudRate NOTIFY baudRateChanged)
    Q_PROPERTY(int dataBits READ dataBits WRITE setDataBits NOTIFY dataBitsChanged)
    Q_PROPERTY(int parity READ parity WRITE setParity NOTIFY parityChanged)
    Q_PROPERTY(int stopBits READ stopBits WRITE setStopBits NOTIFY stopBitsChanged)

public:
    // ConnectionLost is deliberately distinct from Disconnected: the user is kept on
    // the data screen with their last values under a watermark while the app retries,
    // whereas Disconnected means they chose to stop and are sent back to the form.
    enum class ConnectionState { Disconnected, Connecting, Connected, ConnectionLost, Failed };
    Q_ENUM(ConnectionState)

    enum class ConnectionType { Tcp, Rtu };
    Q_ENUM(ConnectionType)

    explicit ConnectionController(QObject *parent = nullptr);
    ~ConnectionController() override;

    ConnectionState state() const;
    QString errorMessage() const;

    // False when the current settings can't produce a connection attempt at all -
    // notably RTU with no serial port selected, which would otherwise fail with an
    // opaque OS-level "cannot find the path specified".
    bool canConnect() const;

    QString host() const;
    void setHost(const QString &host);
    int port() const;
    void setPort(int port);
    int unitId() const;
    void setUnitId(int unitId);
    int timeoutMs() const;
    void setTimeoutMs(int timeoutMs);
    int retryCount() const;
    void setRetryCount(int retryCount);

    ConnectionType connectionType() const;
    void setConnectionType(ConnectionType type);
    QString portName() const;
    void setPortName(const QString &portName);
    int baudRate() const;
    void setBaudRate(int baudRate);
    int dataBits() const;
    void setDataBits(int dataBits);
    int parity() const;
    void setParity(int parity);
    int stopBits() const;
    void setStopBits(int stopBits);

    Q_INVOKABLE void connectToDevice();
    Q_INVOKABLE void disconnectFromDevice();

    // registerType is Core::RegisterType's raw int (Coil/DiscreteInput/
    // HoldingRegister/InputRegister). Bit-type reads emit bitsRead; word-type reads
    // emit holdingRegistersRead (kept generic rather than renamed -- it already
    // covers both Holding and Input register one-shot reads).
    Q_INVOKABLE void readRegisters(int registerType, int startAddress, int quantity);
    Q_INVOKABLE void writeSingleRegister(int address, int value);
    Q_INVOKABLE void writeSingleCoil(int address, bool value);

    // Continuous polling of a register range. Writes still go through
    // writeSingleRegister()/writeSingleCoil(); the poll loop picks the new value up
    // on its next cycle.
    Q_INVOKABLE void startPolling(int registerType, int startAddress, int quantity);

    // Favorites-mode counterpart: polls whatever FavoritesModel currently holds
    // instead of a contiguous range. Exactly one of Normal/Favorites is ever active
    // -- calling either one retargets the single shared PollEngine (its own
    // generation counter drops any in-flight responses from the abandoned mode).
    Q_INVOKABLE void startPollingFavorites(FavoritesModel *favoritesModel);

    Q_INVOKABLE void stopPolling();

    bool isPolling() const;
    int pollIntervalMs() const;
    void setPollIntervalMs(int intervalMs);
    int reconnectIntervalMs() const;
    void setReconnectIntervalMs(int intervalMs);

signals:
    void stateChanged();
    // Distinct from stateChanged(): SessionLogger (M12a) counts genuine
    // disconnect/reconnect events, which a bare state comparison can't tell apart
    // from a fresh initial connect or a repeated failed retry attempt (the latter
    // never changes state at all, see handleTransportError()).
    void connectionLost();
    void connectionRestored();
    void errorMessageChanged();
    void canConnectChanged();
    void hostChanged();
    void portChanged();
    void unitIdChanged();
    void timeoutMsChanged();
    void retryCountChanged();
    void connectionTypeChanged();
    void pollingChanged();
    void pollIntervalMsChanged();
    void reconnectIntervalMsChanged();
    void portNameChanged();
    void baudRateChanged();
    void dataBitsChanged();
    void parityChanged();
    void stopBitsChanged();

    void holdingRegistersRead(int startAddress, QList<int> values);
    void bitsRead(int startAddress, QList<bool> values);
    void singleRegisterWritten(int address, int value);
    void singleCoilWritten(int address, bool value);
    void operationFailed(const QString &message);

    // Normal-mode counterpart to FavoritesModel::markStale(): Normal mode has no
    // model pointer ConnectionController can call directly (unlike Favorites), so
    // it relays through this signal instead, exactly like holdingRegistersRead does.
    void registerReadFailed(const QString &reason);

    // direction matches CommunicationLogModel::Direction's underlying values
    // (0=Tx, 1=Rx, 2=Error) by convention -- same "match by int" style already
    // used for PollModeController.mode/DisplaySettings.addressConvention in QML.
    void communicationLogged(int direction, QString summary);

private:
    void beginPolling(const QList<Core::PollTarget> &targets, FavoritesModel *favoritesModel);

    void setState(ConnectionState newState);
    void handleTransportConnectionStateChanged(bool connected);
    void beginReconnectAttempt();
    void openActiveTransport();
    void handleTransportError(const QString &message);
    void handleResponseReceived(quint64 correlationId, const QByteArray &responsePdu);
    void handleRequestFailed(quint64 correlationId, const QString &reason);

    Core::ITransport *activeTransport() const;

    Core::TcpTransport m_tcpTransport;
    Core::SerialTransport m_serialTransport;
    Core::ModbusTransactionManager m_transactionManager;
    Core::PollEngine m_pollEngine;

    ConnectionState m_state = ConnectionState::Disconnected;
    QString m_errorMessage;

    ConnectionType m_connectionType = ConnectionType::Rtu;

    QString m_host = QStringLiteral("127.0.0.1");
    int m_port = 502;
    int m_unitId = 1;
    int m_timeoutMs = 1000;
    int m_retryCount = 2;
    int m_pollIntervalMs = 1000;
    int m_reconnectIntervalMs = 5000;

    QTimer m_reconnectTimer;

    // Remembered across a connection loss so polling can resume on its own once the
    // device comes back, without the user reconfiguring anything.
    bool m_resumePollingOnReconnect = false;
    Core::RegisterType m_pollRegisterType = Core::RegisterType::HoldingRegister;
    int m_pollStartAddress = 0;
    int m_pollQuantity = 0;

    // Non-owning -- QML owns the FavoritesModel instance's lifetime (destroyed when
    // StackView pops MainScreen back to ConnectionScreen on disconnect, hence
    // disconnectFromDevice() nulling this out). Null means Normal mode is active;
    // set means the relay lambda below routes updates into it instead of emitting
    // holdingRegistersRead. Persists across a connection-loss pause (stopPolling()
    // during a loss does not clear it) so auto-reconnect resumes into whichever mode
    // was active.
    FavoritesModel *m_activeFavoritesModel = nullptr;

    // Defaults per Decision 19: 115200 baud, 8 data bits, Even parity (the Modbus
    // RTU spec's own default), 1 stop bit.
    QString m_portName;
    int m_baudRate = 115200;
    int m_dataBits = 8;
    int m_parity = int(QSerialPort::EvenParity);
    int m_stopBits = 1;

    // Tracks what kind of request is currently in flight so the transaction
    // manager's function-code-agnostic responseReceived signal can be decoded
    // correctly.
    enum class PendingOperation {
        None,
        ReadHoldingRegisters,
        ReadInputRegisters,
        ReadCoils,
        ReadDiscreteInputs,
        WriteSingleRegister,
        WriteSingleCoil
    };
    PendingOperation m_pendingOperation = PendingOperation::None;
    int m_pendingStartAddress = 0;
    // Only meaningful for ReadCoils/ReadDiscreteInputs: decodeReadCoilsResponse/
    // decodeReadDiscreteInputsResponse need the originally-requested quantity to
    // trim the response's padding bits (a byte-packed bit response is padded out to
    // a whole byte).
    int m_pendingQuantity = 0;
    // PollEngine and this controller's one-shot reads/writes share one
    // ModbusTransactionManager, so both handleResponseReceived/handleRequestFailed
    // and PollEngine's own handlers fire for *every* response/failure regardless of
    // which one issued the request. PollEngine filters its own by generation;
    // without an equivalent filter here, a poll response/timeout arriving while a
    // one-shot request is outstanding (e.g. writing a Favorites Coil while that
    // mode is actively polling other entries) was being misread as the answer to
    // this controller's own pending operation, corrupting m_pendingOperation and
    // spuriously emitting operationFailed for poll timeouts that had nothing to do
    // with the one-shot request. Each one-shot send gets a fresh id here, in a
    // counter space disjoint from PollEngine's generation-based ids (which are
    // always >= 1<<32 once running), so the two can never collide.
    quint64 m_pendingCorrelationId = 0;
};

} // namespace ModbusViewer::AppLib
