# Milestone history

Archived narrative for **completed** milestones, oldest first. This file is
deliberately **not** part of the mandatory session-start reading list (see
`CLAUDE.md`'s "Start every session here") — `PROGRESS.md`'s "Current status" only
keeps the most recent 1-2 milestones in full detail; once a newer milestone
finishes, the previous one moves here. Nothing here is stale or superseded, it's
just not needed to know "where things stand right now."

The milestone table in `PROGRESS.md` still gives an at-a-glance list of every
milestone and its status — read this file only when you need the *why* behind a
specific past decision (there's no git history to fall back on for that, since
this project isn't a git repo).

## M1 — Scaffold + navigation shell + docs skeleton

**Done.** Built and launched successfully (Ninja + Qt's bundled MinGW 13.1.0 kit),
user visually confirmed the window renders correctly.

## M2 — Protocol codec (CRC16, MBAP/RTU framing, PDU, exceptions)

**Done.** `core/modbus/` has CRC16, RTU framing (`ModbusRtuFramer`), MBAP framing
(`ModbusTcpFramer`), `ModbusException` decoding, and `ModbusPduCodec` covering all 8
function codes (01,02,03,04,05,06,15,16) with a shared `PduDecodeResult<T>` that
distinguishes success / malformed frame / device exception. 4 Qt Test suites
(`test_crc16`, `test_rtu_framing`, `test_mbap_framing`, `test_pdu_codec`), all
written before their implementation, all green via `ctest`.

## M2a — Transport abstraction (ITransport)

**Done.** `core/transport/ITransport.h` (open/close/isOpen/write/
supportsPipelining + dataReceived/errorOccurred/connectionStateChanged signals).
`tests/support/FakeTransport` is a reusable test double (no real I/O, records what
was written, lets a test simulate received data/errors/pipelining support) —
verified via `test_transport.cpp` with `QSignalSpy`, and earmarked for reuse by the
later PollEngine tests (M5/M5a/M5b) that need controllable latency/ordering. All 5
test suites green.

## M3 — Connect + single read/write over TCP

**Done.** `TcpTransport` (QTcpSocket), `ModbusTransactionManager` (MBAP framing +
transaction-id matching + timeout/retry), `ConnectionController` (QML_SINGLETON with
ConnectionState, host/port/unitId/timeout/retry), `RegisterTableModel`, and real
`ConnectionScreen`/`MainScreen` QML with StackView navigation on connect. Verified
against the pymodbus TCP simulator: connect, read 10 holding registers, and write a
single register all confirmed working end-to-end.

## M4 — RTU/serial transport

**Code complete, live RTU test deferred.** `SerialTransport` (QSerialPort, enforces
3.5-char inter-frame silence in `write()` so callers can't skip it,
`supportsPipelining()` false), `RtuTiming` (t3.5 per spec: 3.5 char times up to
19200 baud, fixed 1.75 ms above), `expectedRtuResponseLength()` (RTU carries no
length field, so frame size is inferred from the function code and byte-count),
`ModbusTransactionManager::FramingMode` {Tcp, Rtu} + `setTransport()`,
`SerialPortListModel` (auto-scan + Rescan), `TcpSettingsPanel`/`RtuSettingsPanel`
QML with a TabBar mode toggle. Everything testable without hardware is done and
green; the end-to-end serial check waits on a real device or a virtual COM pair
(user's choice: "build it now, test RTU later"). **Still to verify on real
hardware:** an actual RTU read/write over a serial link.

## M5 — PollEngine, single-target loop

**Done — user confirmed live values updating in the table.** `core/poll/PollEngine`
polls a `PollTarget` on a `QTimer` at a configurable interval, **gating each cycle
on the previous one finishing** so a device slower than the interval never
accumulates a backlog (unit-tested). Polls immediately on start rather than
waiting an interval, survives timeouts without stalling, and re-arms live when the
interval changes. Wired into `ConnectionController` (`startPolling`/`stopPolling`/
`pollIntervalMs`) with Start/Stop Polling + interval spinbox in `MainScreen`.
`RegisterTableModel::setRegisters` now updates in place and emits `dataChanged`
only for rows that actually changed when the address range is unchanged — a full
model reset every cycle would rebuild all delegates (the fuller batching work is
still M6b).

## M5a — Read Coalescing Engine

**Done.** `core/poll/ReadCoalescer` turns scattered `PollTarget`s into the minimum
set of contiguous read requests. Two clean phases rather than one tangled sweep:
**merge by gap** (never across unit id or register type), then **split by
per-request size limit** (125 registers / 2000 bits, overridable). Coverage is
expressed as explicit slices (`CoveredTarget{targetIndex, offsetInPlan,
offsetInTarget, count}`) so a target split across several plans, a plan carrying
gap-filler values belonging to nobody, and overlapping/duplicate targets all fall
out of one uniform representation with no special cases. 14 test cases, all
written first. Also introduced `core/model/RegisterType.h` (Coil/DiscreteInput/
HoldingRegister/InputRegister) as the grouping key — `PollTarget` now carries a
`RegisterType` instead of a `FunctionCode`, which also simplified `PollEngine`'s
response dispatch and gives M6a's tag database the field it needs.

## M5b — PollEngine pipelining integration

**Done.** The performance architecture from plan Decision 3 is now real.
`ModbusTransactionManager` supports **multiple outstanding requests**, matched by
MBAP transaction id (so out-of-order answers are fine) or positionally for RTU,
each with its own deadline and retry count; callers pass an opaque `correlationId`
that is echoed back, so they identify their own requests without knowing about
transaction ids (which RTU lacks). `PollEngine` now holds a **target set**,
coalesces it into plans each cycle, and issues them with a bounded in-flight
window — `setMaxInFlight` (default 8) for TCP, **automatically forced to 1** when
the transport reports it cannot pipeline. A cycle ends only when every plan has
answered or failed, then the interval timer starts. Decoded values accumulate per
target and are emitted on a **~16ms (60Hz) flush**, so signal volume is bounded by
the flush rate rather than the wire rate (unit-tested: 40 rapid responses produce
far fewer emissions). A **generation counter** is folded into the correlation id so
responses to an abandoned target set are dropped rather than corrupting the new
one. 13 PollEngine + 17 transaction-manager tests.

*Gotcha from this milestone* (also in `CLAUDE.md`'s Recurring gotchas): Qt's
default `Qt::CoarseTimer` rounds to ~5% buckets, so a `QTimer` armed from a
`QDeadlineTimer` could fire *before* that deadline reported itself expired — the
retry then silently slipped a whole cycle. Both the timeout `QTimer` and the
per-request `QDeadlineTimer` now use `Qt::PreciseTimer`, and `rearmTimeoutTimer()`
has a 1ms floor so an early fire reschedules instead of spinning. Protocol
timeouts should always be precise timers.

## M5c — Disconnection handling + auto-reconnect + watermark

**Done.** `ConnectionState` gained `ConnectionLost`, distinct from `Disconnected`:
a hard transport loss (socket/port genuinely closed — not a soft per-row poll
timeout) keeps the user on `MainScreen` with their last values, auto-retries on a
`reconnectIntervalMs` timer (a connection-screen field, default 5000ms), and
resumes the same poll target automatically on success — no reconfiguring. A
user-initiated Disconnect always wins: it stops the reconnect timer outright, and
reappearing hardware won't silently reconnect them
(`userDisconnectDuringConnectionLossStopsReconnectAttempts`, unit-tested). New
`DisconnectedWatermark.qml` (full-screen overlay, Reconnect Now / Give Up) and
`HandshakeAnimation.qml` (shown on the connection screen while Connecting).
`app-lib/` is now also compiled into `tests/` so `test_connection_controller`
could drive this against a **real `QTcpServer`** — genuine socket close, not a
simulated one. 6 new tests, all green.

**Two real bugs this milestone's tests caught before the user would have hit
them:**
1. **Crash on app close while connected.** `~ConnectionController` destroys members
   in reverse declaration order; `~QTcpSocket` (inside `~TcpTransport`) disconnects
   and emits `connectionStateChanged(false)`, whose handler then touched
   `m_reconnectTimer` — already destroyed. `TcpTransport`/`SerialTransport`/
   `ConnectionController` destructors now disconnect their signals first. Any future
   QObject member holding a Qt built-in (socket, port, etc.) needs the same guard.
2. **Re-entrant timeout handling.** `ModbusTransactionManager::handleTimeout()` used
   to retransmit *while iterating* `m_pending`; a synchronous write failure could
   re-enter via `cancelAll()` and clear the list mid-loop. Now decides all mutations
   first, then acts (write/emit) only after the loop is done. General pattern:
   never emit or call out mid-iteration over state that callback could mutate.

Manual verification: killed the pymodbus simulator mid-poll, watermark appeared
over frozen values; restarted it, confirmed auto-recovery. User confirmed working,
and separately noted some minor UI bugs seen during testing that were later
judged "not worth chasing" and were not investigated further (M6 started directly
instead).

## M6 — Value formatting + addressing convention

**Done, user confirmed working.** `core/format/ValueFormatter.h/.cpp` adds
`DisplayFormat` (SignedDecimal/UnsignedDecimal/Hex/Binary/Float32/Int32Signed/
Int32Unsigned) and `ByteOrder` (ABCD/BADC/CDAB/DCBA), pure `formatValue`/
`parseValue` functions with scale/offset/unit applied to the decimal-ish formats
only (Hex/Binary always show the raw bit pattern). `core/format/AddressConvention.h/.cpp`
transforms between 0-based/PDU and 1-based Modicon (4xxxx/3xxxx/1xxxx/0xxxx)
addressing, per `RegisterType`. `RegisterTableModel` reworked around **logical
rows**: a row whose format is Float32/Int32 now consumes its address and the next
one, merging into a single row with a range address (e.g. `40001-40002`) — user
confirmed this "real Modbus Poll" merge behavior over the alternative (two
overlapping rows) during planning. A row that can't pair (multi-register format
landed on the last address) degrades to plain Unsigned Decimal for display, but
`formatSettingsAt()` still returns the originally-requested format so reopening the
picker doesn't silently discard it. New `app-lib/DisplaySettings.h/.cpp`
(`QML_SINGLETON`) holds the one global address-convention toggle; `RegisterTableModel`
mirrors it via a property binding (`registerModel.addressConvention:
DisplaySettings.addressConvention`) rather than reaching for the singleton directly
in C++, matching the existing property-binding wiring style. New
`FormatPicker.qml` (per-row popup, gear button next to each cell) and a header
Address Convention combo, both wired into `MainScreen.qml`. 16 new tests
(`test_value_formatter`, `test_address_convention`, `test_register_table_model`),
all green.

## M6a — Tag database + CSV/JSON parsers

**Done, user confirmed working** (imported 4 tags from both
`tools/sample_tags.csv` and `tools/sample_tags.json` via the "Import Tags..."
button). `core/model/RegisterDefinition.h` is the "Tag" struct (label,
description, registerType, address, reuses M6's `Core::FormatSettings` rather
than duplicating format/byteOrder/scale/offset/unit as separate fields,
`TagSource{Imported,AdHoc}`); `registerSpan()` is derived from the format, not
stored separately, since every v1 format already implies its span.
`core/importer/TagRowParser.h/.cpp` holds one shared field-validation path
(case-insensitive enum matching, required label/registerType/address, optional
format/byteOrder/scale/offset/unit with defaults) used by both
`core/importer/CsvTagParser.h/.cpp` and `JsonTagParser.h/.cpp` so the two file
formats can't validate differently. A malformed row/item is skipped with an
appended error, never aborts the rest of the import. **CSV/JSON schema
(designed this session, not in the original plan):** header row required,
column names case-insensitive and order-independent — `label,description,
registerType,address,format,byteOrder,scale,offset,unit`; JSON is an array of
objects with the same keys. CSV is plain comma-split with **no quoted-field
escaping** (a description containing a literal comma isn't supported in v1 —
flagged as a known limitation, not a bug). New
`app-lib/models/TagDatabaseModel.h/.cpp` (`QAbstractListModel`, `addTags()`
accumulates across multiple imports rather than replacing, `clear()`). New
`app-lib/TagDatabaseController.h/.cpp` (`QML_SINGLETON`): `importCsv`/
`importJson` take the target `TagDatabaseModel*` as a plain argument and call
`addTags()` on it directly in C++, rather than relaying the parsed
`QList<RegisterDefinition>` back out through a signal into QML — that would
need custom metatype registration for a plain struct to survive the QML
boundary, the same category of invisible-failure risk this project's QML
gotchas keep surfacing. Handles `file:///...` URL strings from
`FileDialog.selectedFile` as well as plain paths. New
`app/qml/dialogs/ImportTagFileDialog.qml` (`QtQuick.Dialogs`, picks the parser
by file extension) wired into `MainScreen.qml` via an "Import Tags..." button;
import result (count/errors) shown in the status label. Needed adding
`QuickDialogs2` to `find_package`/`target_link_libraries` — not previously used
by this project. 26 new tests across 6 files, all green
(`test_tag_row_parser`, `test_csv_tag_parser`, `test_json_tag_parser`,
`test_tag_database_model`, `test_tag_database_controller`). No tag-picker UI
yet (Favorites/M6c isn't built); verification for now is the imported
count/error text in the status label.

## M6b — RegisterTableModel batching + Normal view wiring

**Done, user confirmed working.** Investigation at session start found the
milestone's name overstates its remaining scope: the "batching" half was already
fully built and tested (most likely landed ahead of schedule alongside M6/M6a) —
`core/poll/PollEngine`'s 16ms flush timer and `RegisterTableModel::setRegisters()`'s
in-place single-`dataChanged` update for same-shape changes were both already
covered by `test_poll_engine.cpp` and `test_register_table_model.cpp`, and
`ConnectionController`'s relay between them was already exercised end-to-end
(real `PollEngine` against a fake server) in `test_connection_controller.cpp`. So
the only real remaining work was the **"Normal view wiring"** half:
`app/qml/screens/MainScreen.qml`'s Normal-view register list was still a plain
`ListView`, despite `docs/performance.md`'s Decision 9 calling for `TableView`
+ `reuseItems` as the scaling foundation, and `RegisterTableModel::columnCount()`
already returning 2 (address, value) in anticipation of it. Converted to a
`TableView`: column 0 (address, fixed ~90px) and column 1 (value, fills
remaining width) share one delegate that toggles visible content by
`delegateRoot.column` rather than using per-column `Loader`s — simpler for just
two columns. The gear button (format picker) and editable value `TextField` both
live in column 1's `RowLayout`, unchanged in behavior, rather than adding a third
UI-only model column just to host a button. `columnWidthProvider`/
`rowHeightProvider` replace the old `width: registerListView.width`/fixed
`height: 40`; `forceLayout()` is called on completion and width change since
`columnWidthProvider` is a plain JS function Qt doesn't track as a binding — a
resize wouldn't otherwise trigger relayout. No C++/core changes were needed (`data()`
already keyed off `index.row()` only, ignoring column, so both columns' cells see
valid `address`/`value` role values with zero backend change) — per this
project's TDD convention, no new unit needed a written-first test since none of
the three test files above needed modification; `ctest` re-run (still 18/18
green) served as the regression gate, and the user confirmed the TableView
renders, edits, and polls correctly in the running app.

## M6c — Favorites

**Done, user confirmed working.** `FavoritesModel` + `PollModeController` add the
"one mode toggle, one PollEngine" design from plan Decision 6. Investigation
found more reusable groundwork than the milestone's prose implied:
`PollEngine::setTargets()` already bumps a generation counter on every call and
drops stale in-flight responses (already unit-tested in isolation), so no new
`PollTargetSet` class was needed — a plain `QList<PollTarget>` handed to the
existing `setTargets()` was sufficient. `Core::RegisterDefinition` already *was*
the Favorites entry type — its own doc comment names `TagSource::AdHoc` as
"hand-added by the user directly in the Favorites picker (M6c)", designed for
this reuse back in M6a. New `app-lib/models/FavoritesModel.h/.cpp`
(`QAbstractListModel`): `addFromTag(TagDatabaseModel*, row)` (added a small
`TagDatabaseModel::tagAt(row)` C++-only getter so the struct never crosses the
QML boundary, same reasoning as M6a's `TagDatabaseController`) and
`addAdHoc(registerType, address)` (address-as-label, default raw
`UnsignedDecimal` format); `formatSettingsAt`/`setFormatAt`/`setValueAt` match
`RegisterTableModel`'s exact contract, so the existing `FormatPicker.qml`
(duck-typed against a `registerModel` property) works against a `FavoritesModel`
instance with zero QML changes — just a second `FormatPicker` instance bound to
it. `buildPollTargets(unitId)`/`applyRegisterUpdate(targetIndex, ...)` are
C++-only, called directly by `ConnectionController` (never cross the QML
boundary). Scoped to `HoldingRegister`/`InputRegister` only, matching
`RegisterTableModel`'s existing precedent: `Core::formatValue`/`parseValue` only
operate on `QList<quint16>`, and `ConnectionController` never wired up
`PollEngine::targetBitsUpdated`, so there's no bit-value display path anywhere in
the app yet. New `app-lib/PollModeController.h/.cpp` (`QML_SINGLETON`) is
deliberately minimal — just a `Mode{Normal,Favorites}` property, mirroring
`DisplaySettings`'s bare-property-holder pattern (also has no dedicated test
file). Mode switching is coordinated at the QML layer, not in C++: a
`Connections` block in `MainScreen.qml` reacts to `PollModeController.mode`
changing and calls `ConnectionController.startPolling(...)` or the new
`startPollingFavorites(FavoritesModel*)`, hot-swapping the live target set while
polling. `ConnectionController` gained `m_activeFavoritesModel` (non-owning; the
relay lambda from `PollEngine::targetRegistersUpdated` branches on it to route
either into `FavoritesModel::applyRegisterUpdate` or the existing
`holdingRegistersRead` signal) and mode-aware auto-reconnect resume (resumes into
whichever mode was active before the loss); `disconnectFromDevice()` nulls the
pointer since `StackView` destroys the QML `FavoritesModel` instance on pop.
`MainScreen.qml` gained a Mode combo, a `StackLayout` swapping the Normal
`TableView` and a new Favorites `ListView` page (plain `ListView`, not
`TableView` — Favorites is documented as "scaling to 100s," not the thousands
`TableView`'s virtualization targets), an "Add Ad-hoc" row, and an "Add From
Tag..." popup listing `tagDatabaseModel`. 7 new tests in
`tests/test_favorites_model.cpp` plus 2 new integration tests in
`tests/test_connection_controller.cpp` (mode-swap drop behavior and mode-aware
reconnect resume, reusing the existing real-`QTcpServer` fixture) — all green,
19/19 total. User confirmed ad-hoc and tag-backed Favorites both poll and
display correctly, and toggling Normal ↔ Favorites while polling correctly
switches which view's values are live.

## M6d — Search

**Done, user confirmed working.** New `app-lib/models/
RegisterFilterProxyModel.h/.cpp` (`QSortFilterProxyModel`, `QML_ELEMENT`) is one
filter class reused across three source models with different role sets
(`RegisterTableModel`: address/value only; `FavoritesModel`/`TagDatabaseModel`:
label/description/address/unit too) — `filterAcceptsRow()` looks up each
searchable role *by name* via `sourceModel()->roleNames()` at filter time and
silently skips whichever a given source model doesn't expose, rather than
hardcoding one model's role IDs. Plain case-insensitive substring match, not
regex — free-text label search shouldn't need escaping. Never inspects the
`value` role, which is *why* live polling keeps rendering through an active
filter: Qt re-invokes `filterAcceptsRow` on every source `dataChanged`, but
since the predicate never reads `value`, a value-only update can't flip a row's
accept/reject result. Gained a small `Q_INVOKABLE int mapRowToSource(int
proxyRow)` since `QAbstractProxyModel::mapToSource()` itself isn't invokable
from QML — needed because every QML call site that used to pass a delegate's
row straight into `RegisterTableModel`/`FavoritesModel` (`setValueAt`,
`formatSettingsAt`/`setFormatAt` via `FormatPicker.openFor`, `removeAt`) now
binds to a proxy instead, so the delegate's row is the *proxy's* row and must be
mapped back to the source row before those calls or edits would silently hit
the wrong register. `MainScreen.qml` gained three proxy instances
(`normalFilterProxy`, `favoritesFilterProxy`, `tagFilterProxy`) and one toolbar
search field driving the first two simultaneously (the literal "search box
filters both views" verify step); a separate search field was added inside the
existing "Add From Tag" popup, driving `tagFilterProxy`, since the design doc
explicitly calls out reusing the same proxy class for the tag picker too. 9 new
tests in `tests/test_register_filter_proxy_model.cpp` (multi-role filtering
against `FavoritesModel`/`TagDatabaseModel`, filtering against a role-poor
`RegisterTableModel`, `mapRowToSource` with hidden preceding rows, and the
live-update-passthrough guarantee) — all green, 20/20 total. User confirmed
search filters both Normal and Favorites live, editing/format-picking a
filtered (non-first) row hits the correct register, and the tag-picker's own
search field works.

## M7 — Communication log panel

**Done, user confirmed working.** The plan's own milestone entry was the
entire spec ("`CommunicationLogModel` (test-first ring-buffer eviction),
`CommunicationLogPanel.qml`. Verify: every tx/rx frame appears live; disconnect
mid-poll shows a timeout entry.") — schema, panel layout, and buffer size were
all this session's design decisions. **Raw wire bytes didn't exist as a data
path before this milestone**: `ModbusTransactionManager::sendRequest()` built
the framed request and wrote it straight to the transport without ever emitting
it, and the receive path decoded incoming bytes down to a bare PDU before
`responseReceived` fired, discarding the raw MBAP/CRC-framed bytes. Added two
purely-additive signals, `frameSent`/`frameReceived(const QByteArray &rawFrame)`
— emitted in `sendRequest()`, in `handleTimeout()`'s retransmit loop (a retry is
a genuine new wire transmission), and in `consumeTcpFrames()`/`consumeRtuFrames()`
right where the raw frame bytes are already available, before they're stripped
down to a PDU. Since `PollEngine` issues its requests through the same
`ModbusTransactionManager` instance `ConnectionController` already owns, this
one data path covers both poll-driven and one-shot traffic with no separate
hook needed. New `app-lib/CommunicationLogModel.h/.cpp` (`QAbstractListModel`,
`QML_ELEMENT`): fixed 500-entry ring buffer (`append()` evicts the oldest row
once at capacity), `Direction{Tx,Rx,Error}`, timestamp generated internally at
append time. **Two distinct failure paths both had to reach the log** for
"disconnect mid-poll shows a timeout entry" to actually hold: a genuine
protocol-level timeout with retries exhausted
(`ModbusTransactionManager::requestFailed`) *and* a hard transport-level
socket loss (`ITransport::errorOccurred` → `ConnectionController::
handleTransportError()`) — the realistic dev-testing scenario (killing the
simulator) hits the second path, not the first, since a hard socket close
typically errors out before a full timeout elapses; wiring only the first would
have silently failed this milestone's own verify scenario. `ConnectionController`
gained a `communicationLogged(int direction, QString summary)` signal (three new
relay connections to the transaction manager's `frameSent`/`frameReceived`/
`requestFailed`, formatting raw bytes as uppercase hex, plus one line added to
the existing `handleTransportError()`), and `MainScreen.qml` gained a
`CommunicationLogModel` instance, a "Show Log"/"Hide Log" toggle, and a
fixed-height (not resizable — not asked for) collapsible panel with a
timestamp/direction/summary `ListView` (auto-follows new entries via
`onCountChanged: positionViewAtEnd()`) and a "Clear" button. 7 new tests: 3 in
`tests/test_transaction_manager.cpp` (frame-sent/received payloads, retry
re-emits `frameSent`), 3 in `tests/test_communication_log_model.cpp` (append,
ring-buffer eviction, clear), 1 new integration case in
`tests/test_connection_controller.cpp` reusing the existing `FakeModbusServer`
fixture — the automated form of the milestone's literal verify text (shut the
server down mid-poll, confirm a `communicationLogged` emission with
`direction == Error`). All green, 21/21 total. User confirmed live Tx/Rx entries
during both one-shot reads and continuous polling in both modes, Clear/Show/Hide
all work, and killing the simulator mid-poll produces a visible error entry.
