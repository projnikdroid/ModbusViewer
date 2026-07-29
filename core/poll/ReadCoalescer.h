#pragma once

#include <QList>

#include "poll/CoalescingOptions.h"
#include "poll/PollTarget.h"
#include "poll/ReadRequestPlan.h"

namespace ModbusViewer::Core {

// Turns a set of possibly scattered poll targets into the smallest reasonable set of
// contiguous read requests.
//
// This is what makes a Favorites list of hundreds of unrelated addresses practical:
// Modbus can only read contiguous ranges, so without coalescing every favorite would
// cost its own round trip. Normal mode's single contiguous range is just the trivial
// case of the same algorithm - there is no separate code path for it.
class ReadCoalescer
{
public:
    static QList<ReadRequestPlan> coalesce(const QList<PollTarget> &targets,
                                           const CoalescingOptions &options);
};

} // namespace ModbusViewer::Core
