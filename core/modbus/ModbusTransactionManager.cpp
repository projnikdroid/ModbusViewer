#include "ModbusTransactionManager.h"

#include <limits>

#include "ModbusRtuFramer.h"
#include "ModbusTcpFramer.h"

namespace ModbusViewer::Core {

ModbusTransactionManager::ModbusTransactionManager(ITransport *transport, QObject *parent)
    : QObject(parent)
{
    m_timeoutTimer.setSingleShot(true);
    // Coarse timers round to ~5% buckets, which on a short protocol timeout is
    // enough for the timer to fire before its own deadline has expired.
    m_timeoutTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_timeoutTimer, &QTimer::timeout, this, &ModbusTransactionManager::handleTimeout);
    setTransport(transport);
}

void ModbusTransactionManager::setTransport(ITransport *transport)
{
    if (m_transport == transport)
        return;

    if (m_transport)
        disconnect(m_transport, &ITransport::dataReceived, this, &ModbusTransactionManager::handleDataReceived);

    m_transport = transport;
    m_receiveBuffer.clear();

    if (m_transport)
        connect(m_transport, &ITransport::dataReceived, this, &ModbusTransactionManager::handleDataReceived);
}

void ModbusTransactionManager::setFramingMode(FramingMode mode)
{
    m_framingMode = mode;
}

void ModbusTransactionManager::setTimeoutMs(int timeoutMs)
{
    m_timeoutMs = timeoutMs;
}

void ModbusTransactionManager::setRetryCount(int retryCount)
{
    m_retryCount = retryCount;
}

int ModbusTransactionManager::inFlightCount() const
{
    return m_pending.size();
}

bool ModbusTransactionManager::hasRequestInFlight() const
{
    return !m_pending.isEmpty();
}

bool ModbusTransactionManager::supportsPipelining() const
{
    return m_transport && m_transport->supportsPipelining();
}

void ModbusTransactionManager::cancelAll()
{
    m_pending.clear();
    m_receiveBuffer.clear();
    m_timeoutTimer.stop();
}

void ModbusTransactionManager::sendRequest(quint8 unitId, const QByteArray &requestPdu,
                                            quint64 correlationId)
{
    PendingRequest request;
    request.transactionId = ++m_nextTransactionId;
    request.unitId = unitId;
    request.requestPdu = requestPdu;
    request.correlationId = correlationId;

    markAttempt(request);
    m_pending.append(request);
    rearmTimeoutTimer();
    const QByteArray frame = frameFor(request);
    emit frameSent(frame);
    m_transport->write(frame);
}

QByteArray ModbusTransactionManager::frameFor(const PendingRequest &request) const
{
    return m_framingMode == FramingMode::Tcp
        ? encodeMbapFrame(request.transactionId, request.unitId, request.requestPdu)
        : encodeRtuFrame(request.unitId, request.requestPdu);
}

void ModbusTransactionManager::markAttempt(PendingRequest &request)
{
    ++request.attemptsSoFar;
    request.deadline = QDeadlineTimer(m_timeoutMs, Qt::PreciseTimer);
}

void ModbusTransactionManager::rearmTimeoutTimer()
{
    if (m_pending.isEmpty()) {
        m_timeoutTimer.stop();
        return;
    }

    qint64 soonest = std::numeric_limits<qint64>::max();
    for (const PendingRequest &request : m_pending)
        soonest = qMin(soonest, request.deadline.remainingTime());

    // Floor of 1ms so that a timer firing a hair before its deadline reschedules
    // instead of spinning on a zero-length timeout.
    m_timeoutTimer.start(int(qBound(qint64(1), soonest, qint64(std::numeric_limits<int>::max()))));
}

void ModbusTransactionManager::handleDataReceived(const QByteArray &data)
{
    if (m_pending.isEmpty())
        return;

    m_receiveBuffer.append(data);

    if (m_framingMode == FramingMode::Tcp)
        consumeTcpFrames();
    else
        consumeRtuFrames();
}

void ModbusTransactionManager::consumeTcpFrames()
{
    while (!m_pending.isEmpty()) {
        const auto decoded = decodeMbapFrame(m_receiveBuffer);
        if (!decoded.has_value())
            return; // frame incomplete, keep buffering

        const QByteArray rawFrame = m_receiveBuffer.left(decoded->frameByteCount);
        m_receiveBuffer = m_receiveBuffer.mid(decoded->frameByteCount);
        emit frameReceived(rawFrame);

        // Transaction ids let responses come back in any order, so search rather
        // than assuming the head of the queue.
        int matchIndex = -1;
        for (int i = 0; i < m_pending.size(); ++i) {
            if (m_pending.at(i).transactionId == decoded->transactionId) {
                matchIndex = i;
                break;
            }
        }

        if (matchIndex < 0)
            continue; // unknown or already-completed transaction, discard

        completeRequest(matchIndex, decoded->pdu);
    }
}

void ModbusTransactionManager::consumeRtuFrames()
{
    // RTU has no transaction id, so the only possible correlation is positional:
    // whatever arrives answers the oldest outstanding request.
    while (!m_pending.isEmpty()) {
        const int expectedLength = expectedRtuResponseLength(m_receiveBuffer);
        if (expectedLength == 0 || m_receiveBuffer.size() < expectedLength)
            return; // length still undetermined, or frame incomplete

        const QByteArray frame = m_receiveBuffer.left(expectedLength);
        m_receiveBuffer = m_receiveBuffer.mid(expectedLength);
        emit frameReceived(frame);

        const auto decoded = decodeRtuFrame(frame);
        if (!decoded.has_value())
            continue; // bad CRC: drop it and let the timeout drive a retry

        completeRequest(0, decoded->pdu);
    }
}

void ModbusTransactionManager::completeRequest(int pendingIndex, const QByteArray &responsePdu)
{
    const quint64 correlationId = m_pending.at(pendingIndex).correlationId;
    m_pending.removeAt(pendingIndex);
    rearmTimeoutTimer();
    emit responseReceived(correlationId, responsePdu);
}

void ModbusTransactionManager::handleTimeout()
{
    // Finish mutating m_pending before doing anything that can run other code.
    // Writing to a broken socket can emit an error synchronously, whose handler may
    // call cancelAll() - clearing the very list being iterated. Same for emitting
    // failures. So: decide, mutate, then act.
    QList<PendingRequest> expiredAndExhausted;
    QList<QByteArray> framesToRetransmit;

    for (int i = m_pending.size() - 1; i >= 0; --i) {
        if (!m_pending.at(i).deadline.hasExpired())
            continue;

        if (m_pending.at(i).attemptsSoFar <= m_retryCount) {
            markAttempt(m_pending[i]);
            framesToRetransmit.append(frameFor(m_pending.at(i)));
            continue;
        }

        expiredAndExhausted.append(m_pending.at(i));
        m_pending.removeAt(i);
    }

    rearmTimeoutTimer();

    for (const QByteArray &frame : framesToRetransmit) {
        emit frameSent(frame);
        m_transport->write(frame);
    }

    for (const PendingRequest &request : expiredAndExhausted) {
        emit requestFailed(request.correlationId,
                           QStringLiteral("request timed out after %1 attempt(s)")
                               .arg(request.attemptsSoFar));
    }
}

} // namespace ModbusViewer::Core
