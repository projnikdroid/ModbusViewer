#pragma once

#include <QList>
#include <QObject>
#include <QTimer>

#include "modbus/ModbusTransactionManager.h"
#include "poll/CoalescingOptions.h"
#include "poll/PollTarget.h"
#include "poll/ReadCoalescer.h"

namespace ModbusViewer::Core {

// Keeps a set of poll targets live.
//
// Each cycle coalesces the targets into the fewest contiguous requests (see
// ReadCoalescer) and issues them with a bounded number in flight at once - several
// for Modbus TCP, exactly one for RTU, which is half-duplex. A cycle ends when every
// request has answered or failed; only then does the interval timer start, so a
// device slower than the interval degrades to "as fast as it can answer" instead of
// accumulating a backlog.
//
// Decoded values are accumulated per target and emitted on a ~60Hz flush rather than
// per response. Data arriving faster than that simply overwrites the pending value,
// which bounds signal volume no matter how fast the wire is and keeps rendering
// decoupled from polling.
class PollEngine : public QObject
{
    Q_OBJECT

public:
    static constexpr int DefaultMaxInFlight = 8;
    static constexpr int FlushIntervalMs = 16; // ~60Hz

    explicit PollEngine(ModbusTransactionManager *transactionManager, QObject *parent = nullptr);

    void setTargets(const QList<PollTarget> &targets);
    QList<PollTarget> targets() const;

    void setCoalescingOptions(const CoalescingOptions &options);
    void setIntervalMs(int intervalMs);
    int intervalMs() const;

    // Requests allowed outstanding at once. Silently capped to 1 when the transport
    // cannot pipeline, so callers need not special-case RTU.
    void setMaxInFlight(int maxInFlight);
    int effectiveWindowSize() const;

    void start();
    void stop();
    bool isRunning() const;

    int planCount() const;

signals:
    void targetRegistersUpdated(int targetIndex, int startAddress, QList<quint16> values);
    void targetBitsUpdated(int targetIndex, int startAddress, QList<bool> values);
    void pollFailed(const QString &reason);
    // Per-target counterpart to pollFailed, mirroring targetRegistersUpdated's
    // per-target shape -- a coalesced multi-target plan that fails reports every
    // target it was covering, not just one.
    void targetFailed(int targetIndex, const QString &reason);
    void pollCycleCompleted();

private:
    struct TargetCache
    {
        QList<quint16> registerValues;
        QList<bool> bitValues;
        bool dirty = false;
    };

    void rebuildPlans();
    void beginCycle();
    void pumpRequests();
    void handleResponse(quint64 correlationId, const QByteArray &responsePdu);
    void handleFailure(quint64 correlationId, const QString &reason);
    void applyPlanResponse(const ReadRequestPlan &plan, const QByteArray &responsePdu);
    void noteRequestSettled();
    void flushPendingUpdates();
    void markDirty(int targetIndex);

    quint64 makeCorrelationId(int planIndex) const;
    bool isCurrentGeneration(quint64 correlationId) const;
    static int planIndexOf(quint64 correlationId);

    ModbusTransactionManager *m_transactionManager = nullptr;
    QTimer m_intervalTimer;
    QTimer m_flushTimer;

    QList<PollTarget> m_targets;
    QList<ReadRequestPlan> m_plans;
    QList<TargetCache> m_caches;
    CoalescingOptions m_coalescingOptions;

    int m_intervalMs = 1000;
    int m_maxInFlight = DefaultMaxInFlight;
    bool m_running = false;

    // Bumped whenever the target set changes, so responses to abandoned requests
    // from a previous configuration can be recognised and dropped.
    quint32 m_generation = 0;

    int m_nextPlanIndex = 0;
    int m_requestsOutstanding = 0;
};

} // namespace ModbusViewer::Core
