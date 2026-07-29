#include <QTest>

#include "poll/ReadCoalescer.h"

using namespace ModbusViewer::Core;

namespace {

PollTarget target(quint16 address, quint16 quantity,
                  RegisterType type = RegisterType::HoldingRegister, quint8 unitId = 1)
{
    PollTarget t;
    t.unitId = unitId;
    t.registerType = type;
    t.startAddress = address;
    t.quantity = quantity;
    return t;
}

// Total values a plan set requests, i.e. how much wire traffic the coalescing costs.
int totalQuantity(const QList<ReadRequestPlan> &plans)
{
    int total = 0;
    for (const ReadRequestPlan &plan : plans)
        total += plan.quantity;
    return total;
}

} // namespace

class ReadCoalescerTest : public QObject
{
    Q_OBJECT

private slots:
    void emptyInputProducesNoPlans();
    void singleTargetBecomesOnePlanCoveringItExactly();
    void adjacentTargetsMergeIntoOneRequest();
    void targetsWithinGapMergeAndFillerIsAttributedToNobody();
    void targetsBeyondGapStaySeparate();
    void zeroGapMergesOnlyAdjacentTargets();
    void differentRegisterTypesNeverMerge();
    void differentUnitIdsNeverMerge();
    void multiRegisterTargetKeepsCorrectOffset();
    void runExceedingPerRequestLimitIsSplit();
    void targetLargerThanLimitIsSplitAcrossPlansWithContiguousSlices();
    void overlappingTargetsDoNotCorruptOffsets();
    void duplicateTargetsAreBothCovered();
    void bitTypesUseTheLargerPerRequestLimit();
};

void ReadCoalescerTest::emptyInputProducesNoPlans()
{
    QVERIFY(ReadCoalescer::coalesce({}, {}).isEmpty());
}

void ReadCoalescerTest::singleTargetBecomesOnePlanCoveringItExactly()
{
    const QList<PollTarget> targets{target(100, 4)};
    const auto plans = ReadCoalescer::coalesce(targets, {});

    QCOMPARE(plans.size(), 1);
    QCOMPARE(plans[0].startAddress, quint16(100));
    QCOMPARE(plans[0].quantity, quint16(4));
    QCOMPARE(plans[0].registerType, RegisterType::HoldingRegister);
    QCOMPARE(plans[0].covered.size(), 1);
    QCOMPARE(plans[0].covered[0].targetIndex, 0);
    QCOMPARE(plans[0].covered[0].offsetInPlan, 0);
    QCOMPARE(plans[0].covered[0].offsetInTarget, 0);
    QCOMPARE(plans[0].covered[0].count, 4);
}

void ReadCoalescerTest::adjacentTargetsMergeIntoOneRequest()
{
    // [10,12) and [12,14) touch exactly - no filler, always worth merging.
    const QList<PollTarget> targets{target(10, 2), target(12, 2)};
    const auto plans = ReadCoalescer::coalesce(targets, {});

    QCOMPARE(plans.size(), 1);
    QCOMPARE(plans[0].startAddress, quint16(10));
    QCOMPARE(plans[0].quantity, quint16(4));
    QCOMPARE(plans[0].covered.size(), 2);
    QCOMPARE(plans[0].covered[1].targetIndex, 1);
    QCOMPARE(plans[0].covered[1].offsetInPlan, 2);
}

void ReadCoalescerTest::targetsWithinGapMergeAndFillerIsAttributedToNobody()
{
    CoalescingOptions options;
    options.maxGapToBridge = 4;

    // [0,1) and [4,5) - three dead registers between them, within the gap budget.
    const QList<PollTarget> targets{target(0, 1), target(4, 1)};
    const auto plans = ReadCoalescer::coalesce(targets, options);

    QCOMPARE(plans.size(), 1);
    QCOMPARE(plans[0].startAddress, quint16(0));
    QCOMPARE(plans[0].quantity, quint16(5));

    // The bridged registers are read but belong to no target, so nothing claims
    // offsets 1..3.
    QCOMPARE(plans[0].covered.size(), 2);
    QCOMPARE(plans[0].covered[0].offsetInPlan, 0);
    QCOMPARE(plans[0].covered[0].count, 1);
    QCOMPARE(plans[0].covered[1].offsetInPlan, 4);
    QCOMPARE(plans[0].covered[1].count, 1);
}

void ReadCoalescerTest::targetsBeyondGapStaySeparate()
{
    CoalescingOptions options;
    options.maxGapToBridge = 4;

    const QList<PollTarget> targets{target(0, 1), target(100, 1)};
    const auto plans = ReadCoalescer::coalesce(targets, options);

    QCOMPARE(plans.size(), 2);
    QCOMPARE(plans[0].startAddress, quint16(0));
    QCOMPARE(plans[1].startAddress, quint16(100));
    QCOMPARE(totalQuantity(plans), 2); // no wasted reads
}

void ReadCoalescerTest::zeroGapMergesOnlyAdjacentTargets()
{
    CoalescingOptions options;
    options.maxGapToBridge = 0;

    const QList<PollTarget> targets{target(0, 1), target(1, 1), target(3, 1)};
    const auto plans = ReadCoalescer::coalesce(targets, options);

    QCOMPARE(plans.size(), 2);
    QCOMPARE(plans[0].startAddress, quint16(0)); // 0 and 1 are adjacent
    QCOMPARE(plans[0].quantity, quint16(2));
    QCOMPARE(plans[1].startAddress, quint16(3)); // one dead register is still a gap
    QCOMPARE(plans[1].quantity, quint16(1));
}

void ReadCoalescerTest::differentRegisterTypesNeverMerge()
{
    CoalescingOptions options;
    options.maxGapToBridge = 1000;

    // Same addresses, different regions - a single request cannot span both.
    const QList<PollTarget> targets{
        target(0, 1, RegisterType::HoldingRegister),
        target(1, 1, RegisterType::InputRegister),
    };
    const auto plans = ReadCoalescer::coalesce(targets, options);

    QCOMPARE(plans.size(), 2);
    QVERIFY(plans[0].registerType != plans[1].registerType);
}

// Two slaves on the same bus share an address space but need separate requests.
void ReadCoalescerTest::differentUnitIdsNeverMerge()
{
    CoalescingOptions options;
    options.maxGapToBridge = 1000;

    const QList<PollTarget> targets{
        target(0, 1, RegisterType::HoldingRegister, 1),
        target(1, 1, RegisterType::HoldingRegister, 2),
    };
    const auto plans = ReadCoalescer::coalesce(targets, options);

    QCOMPARE(plans.size(), 2);
    QVERIFY(plans[0].unitId != plans[1].unitId);
}

// A float32 tag spans two registers; its slice must report both.
void ReadCoalescerTest::multiRegisterTargetKeepsCorrectOffset()
{
    const QList<PollTarget> targets{target(10, 1), target(11, 2)};
    const auto plans = ReadCoalescer::coalesce(targets, {});

    QCOMPARE(plans.size(), 1);
    QCOMPARE(plans[0].quantity, quint16(3));
    QCOMPARE(plans[0].covered[1].targetIndex, 1);
    QCOMPARE(plans[0].covered[1].offsetInPlan, 1);
    QCOMPARE(plans[0].covered[1].count, 2);
}

void ReadCoalescerTest::runExceedingPerRequestLimitIsSplit()
{
    CoalescingOptions options;
    options.maxRegistersPerRequest = 10;
    options.maxGapToBridge = 0;

    // Eleven adjacent single-register targets: one register past the ceiling.
    QList<PollTarget> targets;
    for (quint16 address = 0; address < 11; ++address)
        targets.append(target(address, 1));

    const auto plans = ReadCoalescer::coalesce(targets, options);

    QCOMPARE(plans.size(), 2);
    QCOMPARE(plans[0].quantity, quint16(10));
    QCOMPARE(plans[1].startAddress, quint16(10));
    QCOMPARE(plans[1].quantity, quint16(1));
    QCOMPARE(plans[0].covered.size(), 10);
    QCOMPARE(plans[1].covered.size(), 1);
    QCOMPARE(plans[1].covered[0].targetIndex, 10);
}

// A Normal-mode range wider than one request must still be fully covered, with the
// slices stitching back together in order.
void ReadCoalescerTest::targetLargerThanLimitIsSplitAcrossPlansWithContiguousSlices()
{
    CoalescingOptions options;
    options.maxRegistersPerRequest = 100;

    const QList<PollTarget> targets{target(0, 250)};
    const auto plans = ReadCoalescer::coalesce(targets, options);

    QCOMPARE(plans.size(), 3);
    QCOMPARE(plans[0].quantity, quint16(100));
    QCOMPARE(plans[1].quantity, quint16(100));
    QCOMPARE(plans[2].quantity, quint16(50));

    int expectedOffsetInTarget = 0;
    for (const ReadRequestPlan &plan : plans) {
        QCOMPARE(plan.covered.size(), 1);
        QCOMPARE(plan.covered[0].targetIndex, 0);
        QCOMPARE(plan.covered[0].offsetInPlan, 0);
        QCOMPARE(plan.covered[0].offsetInTarget, expectedOffsetInTarget);
        expectedOffsetInTarget += plan.covered[0].count;
    }
    QCOMPARE(expectedOffsetInTarget, 250); // every value accounted for exactly once
}

void ReadCoalescerTest::overlappingTargetsDoNotCorruptOffsets()
{
    // [10,14) and [12,16) share registers 12 and 13.
    const QList<PollTarget> targets{target(10, 4), target(12, 4)};
    const auto plans = ReadCoalescer::coalesce(targets, {});

    QCOMPARE(plans.size(), 1);
    QCOMPARE(plans[0].startAddress, quint16(10));
    QCOMPARE(plans[0].quantity, quint16(6)); // union of both, not the sum
    QCOMPARE(plans[0].covered.size(), 2);
    QCOMPARE(plans[0].covered[0].offsetInPlan, 0);
    QCOMPARE(plans[0].covered[0].count, 4);
    QCOMPARE(plans[0].covered[1].offsetInPlan, 2);
    QCOMPARE(plans[0].covered[1].count, 4);
}

void ReadCoalescerTest::duplicateTargetsAreBothCovered()
{
    const QList<PollTarget> targets{target(5, 2), target(5, 2)};
    const auto plans = ReadCoalescer::coalesce(targets, {});

    QCOMPARE(plans.size(), 1);
    QCOMPARE(plans[0].quantity, quint16(2));
    QCOMPARE(plans[0].covered.size(), 2); // neither target is silently dropped
    QCOMPARE(plans[0].covered[0].offsetInPlan, 0);
    QCOMPARE(plans[0].covered[1].offsetInPlan, 0);
}

void ReadCoalescerTest::bitTypesUseTheLargerPerRequestLimit()
{
    // 200 coils fit in one request (limit 2000) where 200 registers would not.
    const QList<PollTarget> targets{target(0, 200, RegisterType::Coil)};
    const auto plans = ReadCoalescer::coalesce(targets, {});

    QCOMPARE(plans.size(), 1);
    QCOMPARE(plans[0].quantity, quint16(200));
}

QTEST_APPLESS_MAIN(ReadCoalescerTest)
#include "test_read_coalescer.moc"
