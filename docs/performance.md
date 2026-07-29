# Performance

## What "never freezes" actually means

A literal "1ms across thousands of registers" isn't physically possible over real
Modbus wire (PDU limits are 125 registers/request, RTU is baud-rate-limited and
half-duplex). The real requirement: the app/engine itself is never the bottleneck —
poll and render as fast as the protocol/hardware allows, UI stays smooth (~60fps)
regardless of load.

## No worker thread for I/O — windowed, pipelined `PollEngine`

`QTcpSocket`/`QSerialPort` are already async under the Qt event loop, so a thread buys
nothing for I/O. Instead:

- **TCP**: MBAP transaction IDs allow multiple outstanding requests on one socket.
  `PollEngine` keeps a bounded in-flight window (default ~8, tunable), refilling as
  responses arrive, matched by transaction ID (order-independent).
- **RTU**: strictly half-duplex — `ITransport::supportsPipelining()` is false for
  `SerialTransport`, forcing the window to 1.
- **Backpressure**: responses land in a per-target pending-update buffer
  (last-write-wins); a single ~16ms (60Hz) `QTimer` flushes it into the model as one
  batched, contiguous-range `dataChanged` emission. Faster-than-60Hz data is simply
  overwritten pre-flush — no unbounded queue growth, polling never gated by render
  speed.
- **Mode-switch safety**: each `PollTargetSet` swap increments a `generation`
  counter; stale-generation responses are dropped.

See plan Decision 3.

## Read coalescing

`core/poll/ReadCoalescer` groups `PollTarget`s by `RegisterType`, sorts by address,
sweep-merges near targets (within `maxGapToBridge`) into contiguous
`ReadRequestPlan`s, splitting any run exceeding the per-function-code max (125
registers, 2000 coils). This is the mechanism that makes scattered Favorites
efficient to poll, and Normal mode's contiguous range is just a trivial case of the
same algorithm. See plan Decision 5.

## TableView performance for thousands of rows

`TableView`'s built-in cell delegate reuse (`reuseItems: true`) is the foundation.
`RegisterTableModel::data()` must be O(1) — a pre-formatted string recomputed only
when raw value or that row's format/byte-order changes, never inside `data()` on
every paint. `dataChanged` batched per flush cycle as contiguous-range emissions, not
per-cell. QML delegates use Qt6 `required property` role bindings directly, minimal
per-cell logic. See plan Decision 9.
