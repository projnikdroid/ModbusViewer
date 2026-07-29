#pragma once

namespace ModbusViewer::Core {

// A Modbus RTU character is 11 bits on the wire: 1 start + 8 data + 1 parity +
// 1 stop (frames without parity use 2 stop bits, so the total is the same).
constexpr int RtuCharacterBits = 11;

// Above 19200 baud the spec fixes the inter-frame delay at 1.750 ms instead of
// letting 3.5 character times keep shrinking, so a fast master cannot outrun the
// slave devices on the bus.
constexpr int FixedInterFrameSilenceMicroseconds = 1750;
constexpr int MaxBaudRateForCalculatedSilence = 19200;

// Time to transmit one character at the given baud rate.
int characterTimeMicroseconds(int baudRate);

// Minimum silence required between RTU frames (t3.5). Skipping this is what makes
// real slaves misframe requests under load, so SerialTransport paces sends by it.
int interFrameSilenceMicroseconds(int baudRate);

} // namespace ModbusViewer::Core
