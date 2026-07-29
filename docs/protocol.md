# Protocol details

## Function codes (v1)

01 Read Coils, 02 Read Discrete Inputs, 03 Read Holding Registers, 04 Read Input
Registers, 05 Write Single Coil, 06 Write Single Register, 15 Write Multiple Coils,
16 Write Multiple Registers. Extensible to custom/vendor codes via the
`IFunctionCodeHandler` registry (see `architecture.md`).

## Exception decoding

`ModbusException` (`core/modbus/`) maps standard exception codes (01 Illegal
Function, 02 Illegal Data Address, 03 Illegal Data Value, 04 Slave Device Failure,
and others) to human-readable messages surfaced in the communication log and
register error state — decoded once in the M2 PDU codec, never passed around as raw
bytes. See plan Decision 16.

## RTU inter-frame silence timing

`SerialTransport` enforces the standard 3.5-character-time minimum gap between RTU
frames, derived from the configured baud rate, before issuing the next request. Not
optional polish — without it, real slaves can misinterpret frame boundaries under
load. Built in from M4, not retrofitted. See plan Decision 15.

## Addressing convention

`core/format/AddressConvention.h/.cpp` is a pure display-layer transform between
0-based/PDU addressing (what's actually on the wire) and 1-based "Modicon" addressing
(the common 4xxxx/3xxxx/1xxxx/0xxxx datasheet convention). One setting controls
rendering/entry everywhere in the UI; the underlying wire address is unaffected. See
plan Decision 17.

## Timeout + retry

`ConnectionController`/`PollTarget` carry explicit `timeoutMs` and `retryCount` per
connection, distinguishing a transient dropped frame (retried) from a genuinely
offline device (surfaced as an error after retries are exhausted). See plan
Decision 18.
