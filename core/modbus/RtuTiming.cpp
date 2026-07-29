#include "RtuTiming.h"

namespace ModbusViewer::Core {

namespace {
constexpr int MicrosecondsPerSecond = 1000000;
} // namespace

int characterTimeMicroseconds(int baudRate)
{
    if (baudRate <= 0)
        return 0;
    return (RtuCharacterBits * MicrosecondsPerSecond) / baudRate;
}

int interFrameSilenceMicroseconds(int baudRate)
{
    if (baudRate <= 0 || baudRate > MaxBaudRateForCalculatedSilence)
        return FixedInterFrameSilenceMicroseconds;

    // 3.5 character times, computed as 35/10 to keep the arithmetic in integers.
    return (35 * RtuCharacterBits * MicrosecondsPerSecond) / (10 * baudRate);
}

} // namespace ModbusViewer::Core
