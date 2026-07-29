#include "PollEngine.h"

namespace ModbusViewer::Core {

namespace {
constexpr int CorrelationGenerationShift = 32;
} // namespace

PollEngine::PollEngine(ModbusTransactionManager *transactionManager, QObject *parent)
    : QObject(parent)
    , m_transactionManager(transactionManager)
{
    m_intervalTimer.setSingleShot(true);
    connect(&m_intervalTimer, &QTimer::timeout, this, &PollEngine::beginCycle);

    m_flushTimer.setSingleShot(true);
    connect(&m_flushTimer, &QTimer::timeout, this, &PollEngine::flushPendingUpdates);

    connect(m_transactionManager, &ModbusTransactionManager::responseReceived, this,
            &PollEngine::handleResponse);
    connect(m_transactionManager, &ModbusTransactionManager::requestFailed, this,
            &PollEngine::handleFailure);
}

void PollEngine::setTargets(const QList<PollTarget> &targets)
{
    m_targets = targets;
    ++m_generation; // any response still in flight now belongs to an old configuration
    // A live retarget (mode switch while already polling) is about to start its own
    // cycle below. The previous target set's interval timer must not survive that --
    // left armed, it can fire beginCycle() a second time on top of the new cycle's
    // still-unanswered request, and unlike setTargets() itself, beginCycle() does not
    // cancelAll() first.
    m_intervalTimer.stop();
    m_transactionManager->cancelAll();
    rebuildPlans();

    m_caches.clear();
    m_caches.resize(m_targets.size());
    for (int i = 0; i < m_targets.size(); ++i) {
        if (isBitRegisterType(m_targets.at(i).registerType))
            m_caches[i].bitValues.resize(m_targets.at(i).quantity);
        else
            m_caches[i].registerValues.resize(m_targets.at(i).quantity);
    }

    if (m_running)
        beginCycle();
}

QList<PollTarget> PollEngine::targets() const
{
    return m_targets;
}

void PollEngine::setCoalescingOptions(const CoalescingOptions &options)
{
    m_coalescingOptions = options;
    rebuildPlans();
}

void PollEngine::rebuildPlans()
{
    m_plans = ReadCoalescer::coalesce(m_targets, m_coalescingOptions);
}

void PollEngine::setIntervalMs(int intervalMs)
{
    if (m_intervalMs == intervalMs)
        return;
    m_intervalMs = intervalMs;

    // Re-arm against the new interval rather than letting a running timer finish
    // counting down the old one.
    if (m_running && m_intervalTimer.isActive())
        m_intervalTimer.start(m_intervalMs);
}

int PollEngine::intervalMs() const
{
    return m_intervalMs;
}

void PollEngine::setMaxInFlight(int maxInFlight)
{
    m_maxInFlight = qMax(1, maxInFlight);
}

int PollEngine::effectiveWindowSize() const
{
    return m_transactionManager->supportsPipelining() ? m_maxInFlight : 1;
}

int PollEngine::planCount() const
{
    return m_plans.size();
}

void PollEngine::start()
{
    if (m_running)
        return;
    m_running = true;
    beginCycle(); // poll immediately so the view is not blank for a whole interval
}

void PollEngine::stop()
{
    m_running = false;
    m_intervalTimer.stop();
    m_flushTimer.stop();
    m_transactionManager->cancelAll();
    m_requestsOutstanding = 0;
}

bool PollEngine::isRunning() const
{
    return m_running;
}

void PollEngine::beginCycle()
{
    if (!m_running || m_plans.isEmpty())
        return;

    m_nextPlanIndex = 0;
    m_requestsOutstanding = 0;
    pumpRequests();
}

void PollEngine::pumpRequests()
{
    const int window = effectiveWindowSize();
    while (m_running && m_nextPlanIndex < m_plans.size() && m_requestsOutstanding < window) {
        const ReadRequestPlan &plan = m_plans.at(m_nextPlanIndex);
        const QByteArray pdu = encodeReadRequest(readFunctionCodeFor(plan.registerType),
                                                 plan.startAddress, plan.quantity);
        ++m_requestsOutstanding;
        m_transactionManager->sendRequest(plan.unitId, pdu, makeCorrelationId(m_nextPlanIndex));
        ++m_nextPlanIndex;
    }
}

quint64 PollEngine::makeCorrelationId(int planIndex) const
{
    return (quint64(m_generation) << CorrelationGenerationShift) | quint64(quint32(planIndex));
}

bool PollEngine::isCurrentGeneration(quint64 correlationId) const
{
    return quint32(correlationId >> CorrelationGenerationShift) == m_generation;
}

int PollEngine::planIndexOf(quint64 correlationId)
{
    return int(quint32(correlationId & 0xFFFFFFFFULL));
}

void PollEngine::handleResponse(quint64 correlationId, const QByteArray &responsePdu)
{
    if (!m_running || !isCurrentGeneration(correlationId))
        return; // answer to an abandoned configuration

    const int planIndex = planIndexOf(correlationId);
    if (planIndex >= 0 && planIndex < m_plans.size())
        applyPlanResponse(m_plans.at(planIndex), responsePdu);

    noteRequestSettled();
}

void PollEngine::handleFailure(quint64 correlationId, const QString &reason)
{
    if (!m_running || !isCurrentGeneration(correlationId))
        return;

    // A failed request must not stall the cycle: report it and keep the loop going.
    emit pollFailed(reason);

    const int planIndex = planIndexOf(correlationId);
    if (planIndex >= 0 && planIndex < m_plans.size()) {
        for (const CoveredTarget &covered : m_plans.at(planIndex).covered)
            emit targetFailed(covered.targetIndex, reason);
    }

    noteRequestSettled();
}

void PollEngine::noteRequestSettled()
{
    m_requestsOutstanding = qMax(0, m_requestsOutstanding - 1);
    pumpRequests();

    const bool cycleFinished = m_nextPlanIndex >= m_plans.size() && m_requestsOutstanding == 0;
    if (!cycleFinished)
        return;

    emit pollCycleCompleted();
    if (m_running)
        m_intervalTimer.start(m_intervalMs);
}

void PollEngine::applyPlanResponse(const ReadRequestPlan &plan, const QByteArray &responsePdu)
{
    if (isBitRegisterType(plan.registerType)) {
        const auto result = plan.registerType == RegisterType::Coil
            ? decodeReadCoilsResponse(responsePdu, plan.quantity)
            : decodeReadDiscreteInputsResponse(responsePdu, plan.quantity);
        if (!result.ok()) {
            emit pollFailed(result.errorMessage);
            for (const CoveredTarget &covered : plan.covered)
                emit targetFailed(covered.targetIndex, result.errorMessage);
            return;
        }

        for (const CoveredTarget &covered : plan.covered) {
            QList<bool> &destination = m_caches[covered.targetIndex].bitValues;
            for (int i = 0; i < covered.count; ++i)
                destination[covered.offsetInTarget + i] = result.value.values.at(covered.offsetInPlan + i);
            markDirty(covered.targetIndex);
        }
        return;
    }

    const auto result = plan.registerType == RegisterType::HoldingRegister
        ? decodeReadHoldingRegistersResponse(responsePdu)
        : decodeReadInputRegistersResponse(responsePdu);
    if (!result.ok()) {
        emit pollFailed(result.errorMessage);
        for (const CoveredTarget &covered : plan.covered)
            emit targetFailed(covered.targetIndex, result.errorMessage);
        return;
    }

    for (const CoveredTarget &covered : plan.covered) {
        QList<quint16> &destination = m_caches[covered.targetIndex].registerValues;
        for (int i = 0; i < covered.count; ++i)
            destination[covered.offsetInTarget + i] = result.value.values.at(covered.offsetInPlan + i);
        markDirty(covered.targetIndex);
    }
}

void PollEngine::markDirty(int targetIndex)
{
    m_caches[targetIndex].dirty = true;
    if (!m_flushTimer.isActive())
        m_flushTimer.start(FlushIntervalMs);
}

void PollEngine::flushPendingUpdates()
{
    for (int i = 0; i < m_caches.size(); ++i) {
        if (!m_caches.at(i).dirty)
            continue;
        m_caches[i].dirty = false;

        const PollTarget &target = m_targets.at(i);
        if (isBitRegisterType(target.registerType))
            emit targetBitsUpdated(i, target.startAddress, m_caches.at(i).bitValues);
        else
            emit targetRegistersUpdated(i, target.startAddress, m_caches.at(i).registerValues);
    }
}

} // namespace ModbusViewer::Core
