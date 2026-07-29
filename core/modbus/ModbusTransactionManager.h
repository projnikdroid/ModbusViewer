#pragma once

#include <QByteArray>
#include <QDeadlineTimer>
#include <QList>
#include <QObject>
#include <QTimer>

#include "transport/ITransport.h"

namespace ModbusViewer::Core {

// Sends request PDUs over a transport, matches responses back to their requests, and
// retries on timeout (Decision 18: explicit timeoutMs/retryCount distinguish a
// transient dropped frame from a genuinely offline device).
//
// Several requests may be outstanding at once. Modbus TCP allows this because the
// MBAP header carries a transaction id, so responses can be matched in any order;
// RTU has no such field and is half-duplex, so there responses are matched
// positionally and the caller is expected to keep only one request in flight.
//
// Callers pass an opaque correlationId that is echoed back with the response, so
// they can identify their own request without knowing anything about transaction
// ids - which RTU does not have.
class ModbusTransactionManager : public QObject
{
    Q_OBJECT

public:
    enum class FramingMode { Tcp, Rtu };

    explicit ModbusTransactionManager(ITransport *transport, QObject *parent = nullptr);

    void setTransport(ITransport *transport);
    void setFramingMode(FramingMode mode);
    void setTimeoutMs(int timeoutMs);
    void setRetryCount(int retryCount);

    void sendRequest(quint8 unitId, const QByteArray &requestPdu, quint64 correlationId = 0);

    int inFlightCount() const;
    bool hasRequestInFlight() const;

    // False for serial transports, which cannot have overlapping transactions.
    bool supportsPipelining() const;

    // Drops every outstanding request without emitting failures - used when the
    // caller abandons the work entirely, e.g. switching poll targets.
    void cancelAll();

signals:
    void responseReceived(quint64 correlationId, const QByteArray &responsePdu);
    void requestFailed(quint64 correlationId, const QString &reason);

    // Raw wire bytes (full MBAP/RTU frame, not the stripped PDU) for the
    // communication log -- fired for every actual transmission/arrival,
    // including retries, regardless of whether a frame ends up matching a
    // pending request.
    void frameSent(const QByteArray &rawFrame);
    void frameReceived(const QByteArray &rawFrame);

private slots:
    void handleDataReceived(const QByteArray &data);
    void handleTimeout();

private:
    struct PendingRequest
    {
        quint16 transactionId = 0;
        quint8 unitId = 0;
        QByteArray requestPdu;
        quint64 correlationId = 0;
        int attemptsSoFar = 0;
        QDeadlineTimer deadline;
    };

    QByteArray frameFor(const PendingRequest &request) const;
    void markAttempt(PendingRequest &request);
    void rearmTimeoutTimer();
    void consumeTcpFrames();
    void consumeRtuFrames();
    void completeRequest(int pendingIndex, const QByteArray &responsePdu);

    ITransport *m_transport = nullptr;
    FramingMode m_framingMode = FramingMode::Tcp;
    QTimer m_timeoutTimer;
    int m_timeoutMs = 1000;
    int m_retryCount = 0;

    quint16 m_nextTransactionId = 0;
    QList<PendingRequest> m_pending;
    QByteArray m_receiveBuffer;
};

} // namespace ModbusViewer::Core
