#pragma once

#include <QList>
#include <QtGlobal>

#include "model/RegisterType.h"

namespace ModbusViewer::Core {

// Maps a slice of one plan's response back onto the target that asked for it.
//
// A slice rather than a whole target because a target larger than the per-request
// ceiling gets split across several plans, and a plan that bridged a gap contains
// filler values belonging to no target at all. Expressing every case as an explicit
// slice keeps the consumer free of special cases.
struct CoveredTarget
{
    int targetIndex = 0;   // index into the PollTarget list handed to coalesce()
    int offsetInPlan = 0;  // where this slice starts within the plan's values
    int offsetInTarget = 0; // where this slice starts within the target's own values
    int count = 0;         // number of values in the slice
};

// A single contiguous read request to put on the wire.
struct ReadRequestPlan
{
    quint8 unitId = 1;
    RegisterType registerType = RegisterType::HoldingRegister;
    quint16 startAddress = 0;
    quint16 quantity = 0;
    QList<CoveredTarget> covered;
};

} // namespace ModbusViewer::Core
