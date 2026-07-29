#include "ReadCoalescer.h"

#include <algorithm>

namespace ModbusViewer::Core {

namespace {

// Targets can only share a request if they address the same region of the same
// device, so this pair is the grouping key.
struct GroupKey
{
    quint8 unitId;
    RegisterType registerType;

    bool operator<(const GroupKey &other) const
    {
        if (unitId != other.unitId)
            return unitId < other.unitId;
        return int(registerType) < int(other.registerType);
    }
    bool operator==(const GroupKey &other) const
    {
        return unitId == other.unitId && registerType == other.registerType;
    }
};

// A maximal span produced by the merge phase, before per-request size limits are
// applied.
struct MergedRun
{
    int startAddress = 0;
    int endAddressExclusive = 0;
    QList<int> targetIndices;
};

QList<MergedRun> mergeByGap(const QList<PollTarget> &targets, const QList<int> &sortedIndices,
                            int maxGapToBridge)
{
    QList<MergedRun> runs;

    for (int index : sortedIndices) {
        const PollTarget &target = targets.at(index);

        const bool startsNewRun = runs.isEmpty()
            || (target.startAddress - runs.last().endAddressExclusive) > maxGapToBridge;

        if (startsNewRun) {
            runs.append({target.startAddress, target.endAddressExclusive(), {index}});
            continue;
        }

        MergedRun &current = runs.last();
        // Targets are address-sorted, but an earlier one may extend past this one
        // (nested/overlapping), so the run's end can only grow.
        current.endAddressExclusive = std::max(current.endAddressExclusive, target.endAddressExclusive());
        current.targetIndices.append(index);
    }

    return runs;
}

// Chops a merged run into request-sized pieces and works out which slice of which
// target each piece carries. Splitting after merging keeps the two concerns apart:
// the merge phase only reasons about gaps, this one only about size.
void appendPlansForRun(const QList<PollTarget> &targets, const MergedRun &run, const GroupKey &key,
                       int maxCountPerRequest, QList<ReadRequestPlan> &plans)
{
    for (int chunkStart = run.startAddress; chunkStart < run.endAddressExclusive;
         chunkStart += maxCountPerRequest) {
        const int chunkEnd = std::min(chunkStart + maxCountPerRequest, run.endAddressExclusive);

        ReadRequestPlan plan;
        plan.unitId = key.unitId;
        plan.registerType = key.registerType;
        plan.startAddress = quint16(chunkStart);
        plan.quantity = quint16(chunkEnd - chunkStart);

        for (int index : run.targetIndices) {
            const PollTarget &target = targets.at(index);
            const int overlapStart = std::max(int(target.startAddress), chunkStart);
            const int overlapEnd = std::min(target.endAddressExclusive(), chunkEnd);
            if (overlapStart >= overlapEnd)
                continue; // this target lies entirely in another chunk

            CoveredTarget covered;
            covered.targetIndex = index;
            covered.offsetInPlan = overlapStart - chunkStart;
            covered.offsetInTarget = overlapStart - int(target.startAddress);
            covered.count = overlapEnd - overlapStart;
            plan.covered.append(covered);
        }

        plans.append(plan);
    }
}

} // namespace

QList<ReadRequestPlan> ReadCoalescer::coalesce(const QList<PollTarget> &targets,
                                               const CoalescingOptions &options)
{
    QList<GroupKey> keys;
    for (const PollTarget &target : targets) {
        const GroupKey key{target.unitId, target.registerType};
        if (!keys.contains(key))
            keys.append(key);
    }
    // Sorted so the output order is deterministic regardless of input order.
    std::sort(keys.begin(), keys.end());

    QList<ReadRequestPlan> plans;

    for (const GroupKey &key : keys) {
        QList<int> indices;
        for (int i = 0; i < targets.size(); ++i) {
            if (GroupKey{targets.at(i).unitId, targets.at(i).registerType} == key)
                indices.append(i);
        }

        std::stable_sort(indices.begin(), indices.end(), [&targets](int lhs, int rhs) {
            return targets.at(lhs).startAddress < targets.at(rhs).startAddress;
        });

        const int maxCount = options.maxCountFor(key.registerType);
        for (const MergedRun &run : mergeByGap(targets, indices, options.maxGapToBridge))
            appendPlansForRun(targets, run, key, maxCount, plans);
    }

    return plans;
}

} // namespace ModbusViewer::Core
