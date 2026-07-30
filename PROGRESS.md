# ModbusViewer — Progress

Single source of truth for where the project stands. Read this first in any new
session, then load only the specific `docs/*.md` topic file you need. The full
rationale behind every decision referenced here lives in the approved plan at
`C:\Users\projn\.claude\plans\zany-sleeping-elephant.md`.

## Current status

**First things next session:**
1. **Check whether the merged CI+CodeQL workflow run actually passed** —
   `gh run view 30420317211 --repo projnikdroid/ModbusViewer` (or `gh run list`
   for whatever's latest on `main`) if this hasn't already been confirmed.
   Carried over from 2026-07-28, still not reverified — see the OSS-readiness
   entry in `docs/history.md` for the full saga if it needs revisiting.

**M10-M10c: Favorites Card View — done (2026-07-29), all layers, user
GUI-verified.** Design rationale and milestone breakdown in the
approved plan at `C:\Users\projn\.claude\plans\quizzical-cuddling-origami.md`
(overwritten in place once the prior V1.1 plan shipped — see `docs/history.md`
for that earlier plan's own narrative). Follows directly from the
reference-image UI survey at the end of M9e (`docs/history.md`): the user
picked "stat card + sparkline" from 3 concept mockups, scoped to Favorites
only, confirmed as a **toggle** (List stays the default, Cards is opt-in) with
**no threshold/severity coloring** in this pass (value + sparkline only —
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

**Two reported "values not updating" symptoms — investigated, not code
bugs.** User reported (1) switching Normal→Favorites while polling appeared
to stall values, and (2) changing a format from Decimal→Hex appeared to stall
values. Investigated the same way as the M8 "unit editable" bug (see
`docs/history.md`): temporary `qDebug()`/`console.log()` diagnostics added to
`ConnectionController`'s poll-relay lambdas, `FavoritesModel::
applyRegisterUpdate`/`applyBitUpdate`/`setFormatAt`, `RegisterTableModel::
setRegisters`/`setFormatAt`, and the QML mode-switch/format-picker handlers;
user reproduced with a Debug console attached. **Root cause for (1)**: the
Favorites list was empty (`targets.size() == 0` in the diagnostic log) at the
moment of switching — `PollEngine` correctly polls nothing when there's
nothing to poll. The "stop → switch → start" workaround that seemed to fix it
actually worked because entries had been added to Favorites in between the
two attempts, not because of the stop/start itself. **Root cause for (2)**:
not reproducible — the diagnostic log showed `setFormatAt` firing correctly
and subsequent poll cycles continuing to update via `RegisterTableModel::
setRegisters`'s `sameRawShape` fast path exactly as designed; user confirmed
on retest it no longer occurred (or was a one-off). All diagnostic logging
reverted afterward — confirmed via `grep -rn "DIAG"` returning nothing — no
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

**Older milestones (M1–M9e, plus the post-M8 punch list, OSS-readiness pass,
and the reference-image UI survey that led into M10):** moved to
`docs/history.md` to keep this section lean — see that file for the full
narrative and design decisions behind each.

## Milestones

| # | Milestone | Status |
|---|-----------|--------|
| M1 | Scaffold + navigation shell + docs skeleton | **Done** |
| M2 | Protocol codec (CRC16, MBAP/RTU framing, PDU, exceptions) | **Done** |
| M2a | Transport abstraction (ITransport) | **Done** |
| M3 | Connect + single read/write over TCP | **Done** |
| M4 | RTU/serial transport | **Code complete** — live serial test deferred (no hardware yet) |
| M5 | PollEngine, single-target loop | **Done** |
| M5a | Read Coalescing Engine | **Done** |
| M5b | PollEngine pipelining integration | **Done** |
| M5c | Disconnection handling + auto-reconnect + watermark | **Done** |
| M6 | Value formatting + addressing convention | **Done** |
| M6a | Tag database + CSV/JSON parsers | **Done** |
| M6b | RegisterTableModel batching + Normal view wiring | **Done** |
| M6c | Favorites | **Done** |
| M6d | Search | **Done** |
| M7 | Communication log panel | **Done** |
| M8 | Polish + packaging | **Done** |
| M9 | V1.1: PollEngine bit-decode regression coverage | **Done** |
| M9a | V1.1: FavoritesModel bit-value plumbing | **Done** |
| M9b | V1.1: RegisterTableModel bit-value plumbing + registerType property | **Done** |
| M9c | V1.1: ConnectionController generalization for all 4 register types | **Done** |
| M9d | V1.1: Normal-mode address-space selector + Coil/DiscreteInput UI | **Done**, user GUI-verified |
| M9e | V1.1: Favorites-mode ad-hoc combo + bit controls | **Done**, user GUI-verified |
| M10 | ValueFormatter::numericValue() | **Done** |
| M10a | FavoritesModel sparkline history buffer + HistoryRole | **Done** |
| M10b | DisplaySettings.favoritesViewMode | **Done** |
| M10c | Favorites Card View QML (GridView + Canvas sparkline) | **Done**, user GUI-verified |

## Environment

- CMake 3.30 — present.
- **Qt 6.11.1 installed at `C:\Qt\6.11.1\mingw_64`**, with Qt's own bundled MinGW
  13.1.0 compiler (`C:\Qt\Tools\mingw1310_64`) and Ninja (`C:\Qt\Tools\Ninja`) — fully
  self-contained, no MSVC needed. The Qt **Serial Port** add-on module is installed
  (added via `C:\Qt\MaintenanceTool.exe` during M4).
  Build/run commands need this on PATH:
  `C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\Ninja;C:\Qt\6.11.1\mingw_64\bin`
- Configure/build: `cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64" -DCMAKE_BUILD_TYPE=Debug`
  then `cmake --build build`. Executable lands at `build/ModbusViewer.exe`.
- Run tests: `cd build && ctest --output-on-failure`. First run of freshly-built
  `.exe`s is slow (~10-25s each) — Windows Defender scanning new executables, not a
  code issue; reruns are fast.
- **GUI verification workflow: the user runs the app and supplies screenshots.**
  Driving the GUI from the agent side (launch + foreground + screenshot + synthetic
  clicks via PowerShell/Win32) works but is far too slow to be worth it. Agent builds
  and runs `ctest`; user does the visual/manual check. Non-visual runtime facts can
  still be checked cheaply from the agent side (e.g. `netstat -an | grep ":502 "`
  confirms the app actually established its Modbus TCP connection).
- MSVC Build Tools 2022 — also installed (via winget) but **unused**; the project
  builds with Qt's MinGW kit instead. Harmless to leave installed.
- Python 3.14 + pymodbus — **installed and working**, but `python` is not on this
  machine's `PATH` — use the full interpreter path,
  `C:\Users\projn\AppData\Local\Python\bin\python.exe` (see `CLAUDE.md`'s "Dev-time
  simulator" section for the exact command). `tools/pymodbus_simulator.py` is a
  TCP/RTU dev-time slave simulator, smoke-tested against a real Modbus TCP read.

## Known rough edges (expected, scheduled — not new bugs)

- **CSV tag import has no quoted-field escaping.** A description containing a
  literal comma will be split incorrectly. Not hit by any current fixture; add
  proper CSV quoting to `CsvTagParser` if a real register map needs it.
- **Writing to a merged (multi-register) row is two sequential single-register
  writes (function code 06), not one atomic Write Multiple Registers (function code
  16).** `ConnectionController` only exposes `writeSingleRegister` today. Works
  functionally but isn't atomic — a slave could see a torn intermediate value
  between the two writes. Add function-code-16 support to `ConnectionController`
  if/when that matters (flagged during M6, out of scope for its verify step).
- RTU is implemented and unit-tested but has never run over a real serial link —
  this machine has no serial ports at all. Needs a device or a com0com virtual pair.

## Notes from implementation (deviations from the plan, decisions made along the way)

- M1: top-level `CMakeLists.txt` only adds `app/` for now (not `core/`, `app-lib/`,
  `tests/`) — those subdirectories have no content yet, and Decision 20's Karpathy
  simplicity bias says not to scaffold unused CMake targets. They'll be added to
  `add_subdirectory(...)` in M2 when `core/` and `tests/` get real content, and in M3
  when `app-lib/` does.
- M1 prerequisite work: the plan's dev-simulator prerequisite (`pymodbus`) turned out
  to need a version pin. pymodbus 3.8+ is mid-migration to a new SimData/SimDevice
  datastore API ahead of v4 and deprecated (with warnings, and some breakage — e.g.
  `ModbusServerContext` is no longer subscriptable) the classic
  `ModbusSlaveContext`/`ModbusServerContext` API this simulator uses. Pinned to
  `pymodbus==3.7.4` (last stable release on the classic API) in
  `tools/requirements.txt` instead of fighting the in-flux new one — this is a dev
  fixture, not an app dependency, so stability mattered more than latest-version.
- M1 build fix: `app/CMakeLists.txt` needed `qt_policy(SET QTP0004 NEW)` before
  `qt_add_executable` — without it, `qt_add_qml_module` warns because our QML files
  live in subdirectories (`qml/theme/`, `qml/screens/`) rather than flat under `qml/`.
- M3 gotcha (QML singletons): `pragma Singleton` inside a `.qml` file is **not**
  enough with `qt_add_qml_module`. Without
  `set_source_files_properties(qml/theme/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)`,
  the generated `qmldir` registers it as a plain type, and every `Theme.*` color
  binding silently resolves to nothing — the app renders with a white background and
  **no QML warning at all**. Verify via the generated qmldir containing
  `singleton Theme ...`, not just by reading the .qml source. Any future QML singleton
  needs the same CMake property.
- M3 gotcha (Q_INVOKABLE): any C++ method QML calls must be `Q_INVOKABLE` — being
  `public` is not enough. `RegisterTableModel::setRegisters()` wasn't, so the QML
  `onHoldingRegistersRead` handler threw a TypeError on its first line and everything
  after it (including the status label update) silently never ran. Symptom was "Read
  does nothing", with the C++ side working perfectly.
- M3 gotcha (silent QML failures): `WIN32_EXECUTABLE TRUE` builds a windowed app with
  no console, so QML runtime errors vanish. Now set to `$<NOT:$<CONFIG:Debug>>` —
  Debug builds keep a console and print QML errors/qDebug; Release stays windowed.
- **Resolved (was a known gap):** `app-lib/` sources are now a shared CMake variable
  (`MODBUSVIEWER_APPLIB_SOURCES`/`_INCLUDE_DIRS` in the top-level `CMakeLists.txt`)
  compiled into both the app target and any test via
  `modbusviewer_add_applib_test(...)` in `tests/CMakeLists.txt`. Not a separate
  library: a static lib carrying `QML_ELEMENT` types would need its own
  `qt_add_qml_module`, which fights with the app's for a single URI, and duplicating
  the moc'd sources across two real binaries is simpler than fighting that. Revisit
  only if `app-lib/` grows enough for the recompile cost to matter.
- M5b gotcha (timer precision): Qt's default `Qt::CoarseTimer` rounds to ~5% buckets,
  so a `QTimer` armed from a `QDeadlineTimer` could fire *before* that deadline
  reported itself expired — the retry then silently slipped a whole cycle. Both the
  timeout `QTimer` and the per-request `QDeadlineTimer` now use `Qt::PreciseTimer`,
  and `rearmTimeoutTimer()` has a 1ms floor so an early fire reschedules instead of
  spinning. Protocol timeouts should always be precise timers.
- M3 gotcha (QML type registration): C++ types exposed with `QML_ELEMENT` from a
  directory outside `app/` need that directory in `target_include_directories` — the
  generated `modbusviewer_qmltyperegistrations.cpp` includes them by bare angle-bracket
  filename (`<ConnectionController.h>`), so `app-lib/` and `app-lib/models/` are on the
  app target's include path.
