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

## M8 — Polish + packaging

**Done, user confirmed working.** Exact plan spec: "theme pass,
error/stale-value states, LICENSE placeholder added, `windeployqt`/
`macdeployqt` smoke test." Investigation found "error/stale-value states"
needed real design, not just a QML flourish: `PollEngine::pollFailed` was a
single global-reason signal with no target index at either emission site,
while `RegisterTableModel`/`FavoritesModel` had no stale role at all — values
just sat forever with zero indication once polling started failing. Added
`PollEngine::targetFailed(int targetIndex, const QString &reason)`, mirroring
`targetRegistersUpdated`'s per-target shape, emitted from both `handleFailure()`
and `applyPlanResponse()`'s two decode-failure branches by iterating the failed
plan's `covered: QList<CoveredTarget>` — so a coalesced multi-target read
correctly marks every target it covered, not just one (test-covered
specifically). **Normal and Favorites needed different granularity for a
structural reason**: Normal mode is always exactly one `PollTarget` covering
the whole visible range (so `RegisterTableModel` got a whole-range `stale`
`Q_PROPERTY` + `markStale()`), while Favorites has one `PollTarget` per entry
(so `FavoritesModel` got a genuine per-row `StaleRole`). `ConnectionController`
relays `targetFailed` exactly like it already relays success — `markStale()`
directly on the held `FavoritesModel*` in Favorites mode, or a new
`registerReadFailed` signal (QML relays it into `registerModel.markStale()`)
in Normal mode, since `ConnectionController` has no direct pointer for that
side. 7 new tests: 3 in `test_poll_engine.cpp` (timeout/decode-failure/
coalesced-failure all emit `targetFailed` correctly), 2 in
`test_register_table_model.cpp`, 2 in `test_favorites_model.cpp`. **Two rounds
of user feedback on the QML visuals after the first pass**: (1) whole-row/table
opacity dimming for "stale" read as broken rather than intentional — replaced
with bold + `Theme.warning` (amber) text on the value field itself, no
dimming; the flash-on-update was also moved off animating the text `color`
(which would have fought with the new stale-color binding) onto a separate
background-tint `Rectangle` behind the row content. (2) The resting value-text
color (`Theme.textPrimary`, a light color meant for text on this app's dark
custom `Rectangle` backgrounds) was nearly invisible because `TextField` renders
its own light control background via Qt Quick Controls' default style, which
nothing in this app themes — switched to plain black. Also fixed the
edit-vs-poll race (`Binding { when: !activeFocus }` guarding the value
`TextField`'s `text`, on both Normal and Favorites) and the connection
screen's TCP-panel stretching (trailing filler `Item` in
`TcpSettingsPanel.qml`). `LICENSE` added (MIT, Nikhil Bangar). Theme pass
needed no remediation — investigation found zero hardcoded colors anywhere
outside `Theme.qml` already. **Packaging is a real kept deliverable, not a
throwaway smoke test** (explicit user ask): Release build deployed via
`windeployqt --qmldir app\qml` into `packaging/windows/dist/`, confirmed
launching with `PATH` stripped to bare `C:\Windows\System32` (no Qt, no
MinGW) — that folder can be copied anywhere and run standalone. Documented in
`packaging/windows/README.md`. macOS (`macdeployqt`) untestable on this
Windows-only machine. No new test suite this milestone (7 new cases folded
into the existing `test_poll_engine`/`test_register_table_model`/
`test_favorites_model` suites) — still 21/21 suites green via `ctest`. User
confirmed the black text fix looks right in both the dev build and the
packaged one, then reported the 5 issues captured in the next entry's punch
list during that same testing pass.

## Post-M8 punch list (2026-07-28)

All 5 items closed this session. Nothing was diagnosed beyond the user's own
description at session start — each item investigated/reproduced before
fixing. 22/22 suites green (21 plus the new `test_rtu_feature_suite`).

1. **Bug: mode-switch sometimes stalls live updates — done.**
   Root cause found by reasoning through `PollEngine`'s generation/timer
   bookkeeping and confirmed with a new regression test before fixing (TDD):
   `PollEngine::setTargets()` (called on every live mode switch, via
   `ConnectionController::startPolling`/`startPollingFavorites`, without an
   intervening `stop()`) bumped the generation and cancelled in-flight
   requests, then — if already running — dispatched a fresh cycle immediately.
   But it never stopped `m_intervalTimer`. If the switch landed shortly after
   a cycle had completed, that *old* target set's interval timer was still
   armed and could fire `beginCycle()` a second time on top of the new
   generation's still-unanswered request — and unlike `setTargets()` itself,
   `beginCycle()` never calls `cancelAll()`, so the second call desynced
   `m_requestsOutstanding` from what was actually in flight, silently wedging
   the cycle-completion bookkeeping until an explicit `stop()` (which does
   clear everything) reset it. Fix: `setTargets()` now stops `m_intervalTimer`
   before dispatching the new cycle. New test in `test_poll_engine.cpp`,
   `retargetingAfterACycleCompletesDoesNotLeaveTheOldIntervalTimerArmed`,
   reproduces the race deterministically (short interval + immediate retarget
   + `QTest::qWait` past the stale timer's deadline) — failed against the old
   code (3 requests written instead of 2, the stale timer's spurious extra
   `beginCycle()`), passes now. All 21/21 suites green.
2. **Feature: a button to enable/disable the per-cell flash-on-update — done.**
   Added `DisplaySettings.flashOnUpdateEnabled` (bool
   `Q_PROPERTY`, default `true`), matching the existing `addressConvention`
   pattern of a global display setting shared across views. `MainScreen.qml`'s
   two `onValueChanged` handlers now gate `flashAnimation.restart()`/
   `favFlashAnimation.restart()` behind it, and a `CheckBox { text: "Flash on
   update" }` was added to the toolbar next to Address/Mode. No dedicated C++
   test added — follows this codebase's existing precedent for
   `DisplaySettings` itself (a plain property with no logic beyond
   read/write/notify; `addressConvention` has none either). All 21/21 suites
   still green.
3. **Feature: default the RTU panel open — done.** Changed
   `ConnectionController::m_connectionType`'s default from `Tcp` to `Rtu`
   (`ConnectionScreen.qml`'s `TabBar.currentIndex` and the settings-panel
   `StackLayout` both already derive from `ConnectionController.connectionType`,
   so no QML change was needed — confirmed by reading `ConnectionScreen.qml`
   first). **This broke 9 of `test_connection_controller.cpp`'s cases** (all
   timed out at 5s waiting for `Connected`, ~45s total) — every test there
   constructs a bare `ConnectionController` and calls `connectToDevice()`
   against a `FakeModbusServer` (real `QTcpServer`) without ever setting
   `connectionType`, so they were silently relying on `Tcp` being the default.
   Caught immediately by the full `ctest` run (this is exactly what running
   the suite after every change is for). Fixed by making those 9 tests
   explicit — each now calls
   `controller.setConnectionType(ConnectionController::ConnectionType::Tcp)`
   right after construction — rather than reverting the feature, since the
   tests' actual intent is "test TCP behavior," not "test whatever the
   default happens to be." All 21/21 suites green after.
4. **Test coverage: RTU has no dedicated feature test suite — done.**
   New `tests/test_rtu_feature_suite.cpp` (6 cases). Scoped
   deliberately narrower than `test_connection_controller.cpp`'s TCP coverage:
   `ConnectionController`'s RTU path goes through a real, non-injectable
   `Core::SerialTransport` (a genuine `QSerialPort`, unlike `TcpTransport` which
   the TCP tests exercise against a real loopback `QTcpServer`) — this machine
   has no serial hardware, so `connectToDevice()` itself stays untestable here,
   same conclusion as M4's original finding. What *is* testable and was the
   actual coverage gap: the read/poll loop, half-duplex enforcement, and write
   path, all driven through genuine CRC-framed wire bytes via `FakeTransport`
   rather than a positional/MBAP stand-in. Cases: (1)
   `rtuIsTheDefaultConnectionTypeAndRequiresAPortNameToConnect` — pure-state
   `canConnect()` coverage requiring no transport I/O at all, doubles as a
   regression guard for item 3's default-to-RTU change; (2)
   `pollingOverRtuDecodesCrcFramedResponses` — `PollEngine` +
   `ModbusTransactionManager(FramingMode::Rtu)` decode a real CRC-framed
   response into target values; (3)
   `pollingOverRtuNeverExceedsOneRequestInFlight` — half-duplex windowing holds
   with RTU framing in the loop, not just the generic non-pipelining-transport
   case `test_poll_engine.cpp` already covered with MBAP; (4)
   `pollingOverRtuCoalescesNearbyTargetsIntoOneRoundTrip` — coalescing survives
   real RTU framing; (5)
   `corruptedCrcDuringPollingTimesOutAndAdvancesTheCycleRatherThanStalling` —
   genuinely RTU-specific (no TCP equivalent): RTU has no transaction id, so a
   corrupted frame is indistinguishable from noise and silently dropped
   (`ModbusRtuFramer::decodeRtuFrame`) rather than rejected-by-ID; only the
   timeout can move the cycle forward, and it must still do so; (6)
   `writeSingleRegisterRoundTripsOverRtuFraming` — function-code-06 round trip
   through `ModbusTransactionManager` directly. All 6 passed on first run —
   no bug surfaced, this was pure coverage. All 22/22 suites green.
5. **Bug: unit field only editable for one format — closed, not
   reproducible.** Read all three candidate sites end-to-end
   (`FormatPicker.qml`'s `ignoresScaleOffsetUnit`, `RegisterTableModel`/
   `FavoritesModel`'s `setFormatAt`/`formatSettingsAt`, and `Core::DisplayFormat`'s
   enum ordering) and found nothing that could produce the reported behavior —
   confirmed by diffing the source `.qml` against both `build/` and
   `build-release/`'s copies *and* their compiled `qmlcache` artifacts
   (mtimes all postdated the source, ruling out a stale-build explanation
   too). User confirmed the symptom by description (only Unsigned Decimal
   editable, both Normal and Favorites views) but a screenshot showed the
   Scale/Offset/Unit row genuinely grayed out for Float32 — contradicting
   `ignoresScaleOffsetUnit`'s Hex/Binary-only condition. Added temporary
   `console.log` diagnostics to `ignoresScaleOffsetUnit` and the row's
   `enabled` binding, rebuilt, and had the user re-test against that fresh
   build: **the field now showed correctly editable for Float32.** Root cause
   was never pinned down precisely, but the rebuild is the common factor —
   most likely the user had been testing against an older executable (the
   M8-era `packaging/windows/dist/ModbusViewer.exe`, or a dev build predating
   some earlier change) rather than a stale source/cache problem, since both
   of those were independently ruled out. Diagnostics reverted
   (`FormatPicker.qml` back to its original form — verified via `git diff`-
   equivalent read-back, no repo yet so done by direct comparison). No
   production code changed for this item. If it resurfaces, get a fresh clean
   build first before any further code reading.

## OSS-readiness pass (2026-07-28)

Full scope. `CLAUDE.md`/plan Decision 11, previously deferred until core
features (through M8) were done. User chose full scope (not just
README+LICENSE+.gitignore+first commit) and confirmed the actual GitHub push
stays a manual step for later — this session only prepared the repo locally.

- **Repo**: `git init -b main` (project had no `.git` anywhere before this),
  first commit `16d1c39` — 121 files, 11,400 insertions, working tree clean.
  Staged content scanned for secret-like patterns (API keys, tokens, private
  key headers) before committing; none found.
- **`.gitignore`**: `build/`, `build-release/` (local build output);
  `packaging/windows/dist/` and `packaging/windows/*.zip` (built deliverables,
  not source); `.claude/settings.local.json` and `.claude/scheduled_tasks.lock`
  (personal/session state, not shared project config); plus the usual Python
  cache and editor-artifact patterns. `.claude/skills/modbusviewer-workflow/
  SKILL.md` **is** committed — it's project-scoped workflow documentation
  `CLAUDE.md` itself references, not personal state.
- **`README.md`**: feature list, requirements, build/run/test instructions,
  dev-simulator usage, links to `PROGRESS.md`/`docs/`/`CONTRIBUTING.md`.
  Screenshots left as an explicit "coming soon" placeholder — no fabricated
  images. No CI/repo badges either: badge URLs need a known `owner/repo` path,
  which doesn't exist yet since the repo isn't pushed anywhere.
- **`CONTRIBUTING.md`**: summarizes `docs/coding-standards.md` (TDD,
  Karpathy simplicity bias, Clean Code naming/structure rules) plus the
  test/PR workflow.
- **`.github/workflows/ci.yml`**: a single job — build + `ctest` + CodeQL C/C++
  analysis, on `windows-latest` via `jurplel/install-qt-action`. Originally two
  separate workflow files (`ci.yml` + `codeql.yml`), each independently
  installing Qt and building; merged into one job on the user's suggestion,
  since CodeQL doesn't need its own separate build — it just needs its tracer
  active during a build, and the one build already produced for `ctest` serves
  both. Cuts the Qt install and the compile step from twice to once per run.
  Also added `cache: true` to `install-qt-action` so the extracted Qt SDK is
  reused across runs instead of re-downloaded from Qt's CDN every time. Kept
  the weekly Monday-06:00-UTC schedule trigger (was CodeQL-only) so the whole
  pipeline, not just security scanning, gets a periodic run even with no
  pushes. Deliberately uses the action's default **MSVC** arch rather than the
  MinGW kit local dev uses — the code has no MinGW-specific dependency, and
  MSVC is the better-supported path for this action on GitHub-hosted Windows
  runners.
  **Pinned to Qt 6.10.3, not local dev's 6.11.1**: the first real push (after
  the user installed and authenticated `gh` CLI) failed both workflows at the
  Qt install step — `aqtinstall` couldn't fetch Windows-desktop metadata for
  6.11.1 ("Failed to locate XML data for Qt version"). A first fix attempt
  (`mirror: "https://download.qt.io"` input) was itself wrong —
  `jurplel/install-qt-action@v4` has no `mirror` input, silently ignored per
  the run's own warning annotation, confirmed via a screenshot the user
  shared. Root-caused properly by testing `aqt list-qt windows desktop --arch
  <version>` directly against multiple versions from a completely different
  network (this machine, not the GH runner): 6.11.0/6.11.1/6.12.0 all fail
  identically, but 6.10.3, 6.8.1, and 6.5.3 all resolve fine, and `linux
  desktop --arch 6.11.1` also resolves fine — isolating this to a
  still-propagating CDN gap specific to Windows builds of the three newest Qt
  point releases as of 2026-07-29, not a mirror/cache/config problem. Revisit
  the pin (bump back to 6.11.1) once `aqt list-qt windows desktop --arch
  6.11.1` resolves.
- **`packaging/windows/ModbusViewer-0.1.0-win64.zip`**: zipped the M8
  `dist/` deliverable (38.7 MB) as a release-ready artifact per the user's
  request — gitignored like `dist/` itself, sitting locally ready to attach
  to a GitHub Release.
- **Repo published**: user installed and authenticated `gh` CLI mid-session
  (wasn't available for the initial local-only pass) and asked to proceed with
  the actual publish. Created via `gh repo create ModbusViewer --public
  --source=. --remote=origin --push`, now live at
  `https://github.com/projnikdroid/ModbusViewer`.
- **`/modbusviewer-workflow` skill built**: combined session-start +
  finish-milestone checklist (the design that used to be a "TODO next
  session" section in `PROGRESS.md` — removed once implemented, see the
  skill file itself for the procedure).

## M9-M9e — V1.1: full Modbus register-type support (2026-07-29)

**Done, user GUI-verified.** Full design rationale and the milestone
breakdown lived in the approved plan at
`C:\Users\projn\.claude\plans\quizzical-cuddling-origami.md` (superseded by
the M10 plan in that same file once this series shipped). User's ask had two
parts: (1) wire up all 4 Modbus register types (Coil/DiscreteInput/
HoldingRegister/InputRegister), not just Holding Registers, and (2) react to
a UI-style reference image (dark theme, pill buttons, toggle groups,
gradient sliders) for a "modern, futuristic" look, folded into the same pass
rather than deferred, since the boolean Coil/DiscreteInput controls part (1)
needs *anyway* (a 1-bit value has no text-field representation) map directly
onto that reference's toggle-switch/status-pill language.

Investigation before planning found the protocol codec
(`core/modbus/ModbusPduCodec`) and poll-scheduling layer (`Core::RegisterType`
enum, `PollTarget.registerType`, `PollEngine::targetBitsUpdated`,
`ReadCoalescer` grouping by `(unitId, registerType)`) already fully supported
all four types from earlier milestones — the actual gap was narrow:
`ConnectionController`'s public API, `RegisterTableModel`, and
`FavoritesModel` were hardcoded/scoped to Holding Registers only, and no UI
exposed an address-space selector. Three additional pre-existing gaps
surfaced during planning, not previously known: `PollEngine`'s bit-decode
path had shipped with zero test coverage; `RegisterDefinition::registerSpan()`
ignored `registerType` entirely (a bit-type tag with a stray Float32 format
would have reported span 2, corrupting `FavoritesModel`'s poll-target sizing);
and `DisplaySettings::toDisplayAddress`/`toPduAddress` were hardcoded to
`HoldingRegister`, which would have shown the wrong Modicon prefix once
Normal mode could select other types.

- **M9** (`tests/test_poll_engine.cpp`): closed the `PollEngine` bit-decode
  test gap before building anything on top of it (TDD, no exceptions) — 3 new
  cases covering Coil/DiscreteInput decode and a coalesced bit+word mixed
  cycle.
- **M9a** (`core/model/RegisterDefinition.h`, `app-lib/models/FavoritesModel`):
  fixed the `registerSpan()` bug (`isBitRegisterType(registerType) ? 1 : ...`);
  added bit-value storage (`Entry::bitValue`), three new roles
  (`IsBitRole`/`BoolValueRole`/`WritableRole`), `applyBitUpdate()`,
  `setBitAt()` (Coil-only, no-op on DiscreteInput), and a new
  `coilWriteRequested` signal. New `isWritableRegisterType()` added to
  `core/model/RegisterType.h/.cpp` alongside the existing
  `isBitRegisterType()`/`readFunctionCodeFor()`, reused by both models rather
  than duplicated. 6 new tests in `test_favorites_model.cpp`.
- **M9b** (`app-lib/models/RegisterTableModel`): added a `registerType`
  `Q_PROPERTY` (model-local enum matching `Core::RegisterType`'s ordinals 1:1,
  same int-bridging idiom as `AddressConvention`), bit-value storage, the same
  three new roles, `setBits()`/`setBitAt()` mirroring the existing
  `setRegisters()`/`setValueAt()` split (same-shape diff-in-place vs.
  full-reset, preserving flash-on-update), and fixed `AddressRole` to use the
  model's own register type instead of the hardcoded `HoldingRegister`. 6 new
  tests in `test_register_table_model.cpp`.
- **M9c** (`app-lib/ConnectionController`): `readHoldingRegisters`/
  `startPolling` generalized to `readRegisters(registerType, ...)`/
  `startPolling(registerType, ...)`; new `writeSingleCoil()`; `PendingOperation`
  enum extended with `ReadCoils`/`ReadDiscreteInputs`/`WriteSingleCoil`; a
  second `PollEngine::targetBitsUpdated` relay connected alongside the
  existing `targetRegistersUpdated` one (routes to `FavoritesModel::
  applyBitUpdate()` or a new `bitsRead` signal, same pattern as the existing
  register relay); reconnect-resume now remembers `registerType` alongside
  address/quantity so auto-reconnect resumes into the same address space.
  `holdingRegistersRead` was deliberately **kept**, not renamed, as the
  generic word-register-read signal (now also covers Input Register one-shot
  reads) — avoids unnecessary churn across QML/tests for a signal whose name
  was already generic enough. `tests/test_connection_controller.cpp`'s
  `FakeModbusServer::respond()` fixture hardened to branch on the request's
  function-code byte (bit-packed response for FC01/02, request-echo for FC05,
  existing behavior unchanged for FC03/04/06) — required before any
  Coil/DiscreteInput test could exercise it correctly. 4 new tests, including
  a regression guard that reconnect resumes with the *same* register type
  that was active, not silently falling back to Holding Register.
- **M9d** (`app\qml\screens\MainScreen.qml`, `app-lib/DisplaySettings`):
  `DisplaySettings::toDisplayAddress`/`toPduAddress` gained a `registerType`
  parameter (delegating to the already-register-type-aware
  `Core::displayAddress`/`pduAddress`); new "Type:" `ComboBox` in the toolbar
  next to Address/Mode, live-retargeting a running poll on change (confirmed
  decision, same precedent as the existing Normal↔Favorites mode switch); the
  register table's value column now branches per row on `isBit`/`writable`:
  a `Switch` for Coil (writes immediately on toggle, confirmed decision,
  matching the existing TextField's write-on-editingFinished pattern), a
  read-only ON/OFF status `Text` for DiscreteInput, or the existing
  `TextField` for Holding/Input Register (`readOnly` for InputRegister); the
  format-picker gear button is hidden entirely (not shown-disabled) for bit
  rows, since scale/offset/unit/byteOrder have no meaning for a 1-bit value.
- **M9e** (`MainScreen.qml`): the Favorites "Add Ad-hoc" combo's
  Coil/DiscreteInput exclusion (a deliberate v1 scope cut, see its own removed
  comment) lifted now that a bit-value display/write path exists; the
  Favorites list delegate got the same three-way Switch/pill/TextField
  branching as M9d's table delegate, plus a filler `Item` so the "✕" remove
  button stays right-aligned regardless of which control the row shows.

22/22 suites green after every milestone (M9 through M9e), confirmed via full
rebuild + `ctest --output-on-failure` each time, not just the touched suite.
No GUI regressions in existing Holding-Register flows — `RegisterTableModel`/
`FavoritesModel` default to `HoldingRegister`, matching pre-V1.1 behavior
exactly when the new selector is left untouched. **User GUI-verified
(2026-07-29)**, full checklist against the live simulator: Normal-mode Type
selector + Modicon prefixes, Coil toggle writes immediately and reflects on
next poll, Discrete Input/Input Register read-only, live retarget on
register-type switch while polling, Favorites' ad-hoc combo's new
Coil/DiscreteInput options with the same toggle/pill rows, and no regression
in existing Holding Register flows — all confirmed working.

**Reference-image UI survey that led into this work**: earlier the same
session, the user shared a dark-themed UI reference image (pill buttons,
toggle groups, gradient "Alarms"/"Warning" sliders) as inspiration. Three
concept mockups for numeric register value display were built as a
comparison Artifact — radial gauge, inline level bar, and stat card +
sparkline — each rendered in the app's actual `Theme.qml` palette with one
register live-updating so the user could react to motion, not just resting
state. User picked the stat-card-with-sparkline concept, scoped to Favorites
only; see the M10 series (next entry) for its implementation. The boolean
Coil/DiscreteInput toggle-switch/status-pill controls built in M9d/M9e above
already deliver that reference image's visual language for the register
types that needed new UI in this pass; the numeric-value concepts were the
separate, deferred half of the same ask.

## M10-M10c — Favorites Card View (2026-07-29)

**Done, all layers, user GUI-verified.** Design rationale and milestone
breakdown lived in the approved plan at
`C:\Users\projn\.claude\plans\quizzical-cuddling-origami.md` (overwritten in
place once the prior V1.1 plan shipped). Follows directly from the
reference-image UI survey at the end of M9e above: the user picked "stat card
+ sparkline" from 3 concept mockups, scoped to Favorites only, confirmed as a
**toggle** (List stays the default, Cards is opt-in) with **no
threshold/severity coloring** in this pass (value + sparkline only —
thresholds are an explicit future follow-up, not built now).

Investigation before planning confirmed: no `QtQuick.Shapes`/`QuickShapes`
module linked (`Canvas` needs zero CMake changes, bundled in base `QtQuick`);
`DisplaySettings` (QML_SINGLETON) already holds `addressConvention`/
`flashOnUpdateEnabled` as the precedent pattern for a third cross-cutting
toggle; `FavoritesModel::applyRegisterUpdate`/`applyBitUpdate` are the only
two write-into-`Entry` paths; `RegisterFilterProxyModel` passes through
arbitrary source-model roles unmodified (plain `QSortFilterProxyModel`, no
`roleNames()` override), so a new role reaches a card delegate with zero
proxy-side change.

- **M10** (`core/format/ValueFormatter.h/.cpp`): new `numericValue()` —
  the scale/offset-applied `double` `formatValue()` was computing internally
  and immediately stringifying, now exposed as its own function (raw unsigned
  register value for Hex/Binary, matching `formatValue()`'s existing "no
  scale/offset there" rule). `formatValue()`'s numeric branches refactored to
  call it — **zero behavior change**, the full pre-existing
  `test_value_formatter.cpp` suite passed unmodified after the refactor,
  confirmed by rerun. 2 new tests.
- **M10a** (`app-lib/models/FavoritesModel`): `Entry::history` (`QList<double>`,
  capped at a fixed `kHistoryCapacity = 20` — not poll-interval-adaptive, just
  a private implementation constant) + new `HistoryRole` exposed as a
  `QVariantList`. Appended inside `applyRegisterUpdate()` via the new
  `numericValue()`, guarded by `isBitRegisterType()` (bit entries have no
  trend to plot — defensive, since `applyBitUpdate` is the real path for
  those). Cleared in `setFormatAt()` alongside the existing `rawValues`
  zero-fill (old points are meaningless under a new format/scale). 4 new
  tests, including the eviction-window boundary at exactly 20 points.
- **M10b** (`app-lib/DisplaySettings`): `favoritesViewMode` property
  (`enum class { List, Cards }`), exact shape of the existing
  `addressConvention` pattern, defaults to `List`. No dedicated test — matches
  this project's existing precedent for `DisplaySettings`' other thin
  properties.
- **M10c** (`app/qml/screens/MainScreen.qml`): new "View:" `ComboBox`
  (List/Cards) in the Favorites-page toolbar (next to "Add Ad-hoc"/"Add From
  Tag..." — Favorites-only setting, not the global cross-mode toolbar). The
  Favorites view area is now a nested `StackLayout` (`currentIndex:
  DisplaySettings.favoritesViewMode`) with the existing `favoritesListView`
  unchanged plus a new `GridView` (`cellWidth: 200, cellHeight: 120`) on the
  same `favoritesFilterProxy` — search filtering keeps working unchanged in
  both views. Card delegate mirrors the list delegate's Switch/pill/TextField
  branching (Coil → writable `Switch`, DiscreteInput → read-only ON/OFF pill,
  word types → big number + sparkline), plus a header row with the existing
  ⚙/✕ actions. Sparkline is a `Canvas` (chosen over `Shape` specifically to
  avoid new CMake/module surface), self-relative min/max per card (`||1`
  guard against divide-by-zero when flat), `pts.length < 2` → blank canvas
  for fresh entries, repainted via `Connections { onHistoryChanged:
  sparkline.requestPaint() }` since Canvas doesn't auto-redraw on property
  changes. Line color is `Theme.accent` — no severity system exists yet, and
  accent is already this app's "live/changed data" color (the row-update
  flash). No automated test (QML is GUI-verified by the user, per this
  project's established convention).

22/22 suites green after every milestone (M10 through M10c), full rebuild +
`ctest --output-on-failure` each time. Headless launch sanity check after
M10c (no QML errors printed to the Debug console) — the QML change in M10c
was large enough (a new nested `StackLayout` + `GridView` + `Canvas`
delegate, hand-balanced braces) to be worth that extra check beyond the
usual build-clean signal. **User GUI-verified**: all card-view functionality
(toggle, sparkline growth, Coil/DiscreteInput cards, ⚙/✕, search filter
preserved across views) confirmed working.

## Post-M10c investigations: "values not updating" reports and a real polling gap (2026-07-29/30)

**Two reported "values not updating" symptoms — investigated, not code
bugs.** User reported (1) switching Normal→Favorites while polling appeared
to stall values, and (2) changing a format from Decimal→Hex appeared to stall
values. Investigated the same way as the M8 "unit editable" bug: temporary
`qDebug()`/`console.log()` diagnostics added to `ConnectionController`'s
poll-relay lambdas, `FavoritesModel::applyRegisterUpdate`/`applyBitUpdate`/
`setFormatAt`, `RegisterTableModel::setRegisters`/`setFormatAt`, and the QML
mode-switch/format-picker handlers; user reproduced with a Debug console
attached. **Root cause for (1)**: the Favorites list was empty
(`targets.size() == 0` in the diagnostic log) at the moment of switching —
`PollEngine` correctly polls nothing when there's nothing to poll. The "stop
→ switch → start" workaround that seemed to fix it actually worked because
entries had been added to Favorites in between the two attempts, not because
of the stop/start itself. **Root cause for (2)**: not reproducible — the
diagnostic log showed `setFormatAt` firing correctly and subsequent poll
cycles continuing to update via `RegisterTableModel::setRegisters`'s
`sameRawShape` fast path exactly as designed; user confirmed on retest it no
longer occurred (or was a one-off). All diagnostic logging reverted
afterward — confirmed via `grep -rn "DIAG"` returning nothing — no
production code changes were needed for either symptom.

**Fixed: the empty-Favorites-list finding above surfaced a real, previously
just-documented gap (M6c's "Known rough edges" item) — fixed now, not just
flagged.** User pointed out the UX consequence directly: switch to Favorites
while polling with an empty list (correctly polls nothing), then add an
entry — the new entry silently never gets polled until the user manually
stops/restarts, since `FavoritesModel::buildPollTargets()` was only ever
called at `startPollingFavorites()` time, not on every mutation. Fixed in
`MainScreen.qml` with a small `root.retargetFavoritesPollingIfActive()`
helper (re-calls `ConnectionController.startPollingFavorites(favoritesModel)`
if `ConnectionController.polling && PollModeController.mode === 1`, reusing
the same live-retarget path already proven safe by the register-type-switch
and mode-switch handlers) called after all 4 mutation sites: "Add Ad-hoc",
"Add From Tag...", and both the list-view and card-view "✕" remove buttons.
New regression test in `test_connection_controller.cpp`,
`reRequestingFavoritesPollingAfterAddingAnEntryPicksUpTheNewTarget`, proves
the underlying re-targeting call actually delivers data for a newly-added
entry rather than being a no-op. 22/22 suites green (14 in
`test_connection_controller.cpp`, up from 13). Headless launch check clean
after the QML change.

**Bug fixed: `ConnectionController`'s one-shot requests had no correlation-id
isolation from `PollEngine`'s traffic, sharing the same
`ModbusTransactionManager`.** Found from a real user report: toggling a
Favorites Coil while another Favorites entry was actively polling caused the
other entry to go stale (orange) and Normal mode to show a spurious "request
timed out," persisting until the simulator itself was restarted.
Root cause: `ConnectionController::handleResponseReceived`/
`handleRequestFailed` are connected to `ModbusTransactionManager::
responseReceived`/`requestFailed` — signals `PollEngine` *also* connects to
for its own poll traffic on the same shared transaction manager — but neither
handler checked the `correlationId` against its own currently-pending
one-shot request before acting. So an unrelated poll target's own timeout
(already correctly handled by `PollEngine`'s own, generation-filtered
handler) would *also* trip `ConnectionController`'s handler, which
unconditionally reset `m_pendingOperation` to `None` and emitted
`operationFailed` — misattributing an unrelated poll failure as this
controller's own one-shot request failing. Worse: if that happened while a
real one-shot response (e.g. the coil write's own answer) was still in
flight, it arrived to find `m_pendingOperation` already reset to `None` and
was silently dropped — no `singleCoilWritten`, no refresh-read, the write
appearing to just vanish. This predates this session (the same race applied
to Normal-mode `writeSingleRegister` while polling), but Favorites polling
with a Coil write made it reliably reproducible. **Fix**: `ConnectionController`
now assigns each one-shot request its own `m_pendingCorrelationId` (a
monotonic counter, numerically disjoint from `PollEngine`'s
generation-based ids), and both handlers now reject any response/failure
whose `correlationId` doesn't match. New regression test in
`test_connection_controller.cpp`,
`unrelatedPollTimeoutDuringAOneShotCoilWriteDoesNotCorruptTheWriteResponse`
— extends `FakeModbusServer` with a `setBlackholeReadCoils()` toggle so a
specific poll target can be made to reliably time out on every cycle while a
one-shot coil write is concurrently outstanding. **Verified TDD-style, not
just written**: temporarily disabled the two new guard checks, confirmed the
test actually fails (not just a tautology), then restored the fix and
confirmed it passes — 22/22 suites green (15 in
`test_connection_controller.cpp` now). Headless launch check clean.
