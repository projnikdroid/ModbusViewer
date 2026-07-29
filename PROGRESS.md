# ModbusViewer — Progress

Single source of truth for where the project stands. Read this first in any new
session, then load only the specific `docs/*.md` topic file you need. The full
rationale behind every decision referenced here lives in the approved plan at
`C:\Users\projn\.claude\plans\zany-sleeping-elephant.md`.

## Current status

**First things next session:**
1. **Check whether the merged CI+CodeQL workflow run actually passed** —
   `gh run view 30420317211 --repo projnikdroid/ModbusViewer` (or `gh run list`
   for whatever's latest on `main`). It was still running when the user logged
   off for the night (~13+ min in, longer than the earlier separate runs since
   this one job now does Qt install + CodeQL-traced build + `ctest` + analysis
   all in sequence — see the OSS-readiness entry below for the full saga). If
   it failed, check logs with `gh run view <id> --log-failed` before assuming
   it's the same already-fixed Qt-CDN issue.
2. **UI redesign discussion — planning only, nothing implemented yet.** User
   wants to move Favorites (and possibly Normal) toward a "modern, futuristic"
   look: gauges/level indicators per register instead of (or alongside) the
   current plain table row. A first concept mockup (radial gauges + a compact
   level-bar alternate view, built directly on `Theme.qml`'s existing dark
   palette) was shared as an Artifact and the user liked it, but explicitly
   asked to **hold off on implementation** — next session is for surveying
   several more directions/options before settling on one. Bring 2-3 more
   concept variations (different gauge styles, layout density, motion
   language) to react to, not just one. This is exploratory design work, not
   a coded milestone yet — use plan mode once a direction is actually chosen
   and it's time to design the real QML implementation.

**M8 done — this was the last milestone in the original table.** All further
work is either a bug fix or a new post-v1 item (see `docs/roadmap.md`).

**Punch list: all 5 items closed this session (2026-07-28).** Nothing was
diagnosed beyond the user's own description at session start — each item
investigated/reproduced before fixing. 22/22 suites green (21 plus the new
`test_rtu_feature_suite`). See each item's entry below for what changed (items
1-4) or why item 5 turned out not to be a code bug.

1. **Bug: mode-switch sometimes stalls live updates — done (2026-07-28).**
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
2. **Feature: a button to enable/disable the per-cell flash-on-update — done
   (2026-07-28).** Added `DisplaySettings.flashOnUpdateEnabled` (bool
   `Q_PROPERTY`, default `true`), matching the existing `addressConvention`
   pattern of a global display setting shared across views. `MainScreen.qml`'s
   two `onValueChanged` handlers now gate `flashAnimation.restart()`/
   `favFlashAnimation.restart()` behind it, and a `CheckBox { text: "Flash on
   update" }` was added to the toolbar next to Address/Mode. No dedicated C++
   test added — follows this codebase's existing precedent for
   `DisplaySettings` itself (a plain property with no logic beyond
   read/write/notify; `addressConvention` has none either). All 21/21 suites
   still green.
3. **Feature: default the RTU panel open — done (2026-07-28).** Changed
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
4. **Test coverage: RTU has no dedicated feature test suite — done
   (2026-07-28).** New `tests/test_rtu_feature_suite.cpp` (6 cases). Scoped
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
   reproducible (2026-07-28).** Read all three candidate sites end-to-end
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

**OSS-readiness pass: done (2026-07-28), full scope.** `CLAUDE.md`/plan
Decision 11, previously deferred until core features (through M8) were done.
User chose full scope (not just README+LICENSE+.gitignore+first commit) and
confirmed the actual GitHub push stays a manual step for later — this session
only prepared the repo locally.

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

**`/modbusviewer-workflow` skill: built** (2026-07-28) at
`.claude/skills/modbusviewer-workflow/SKILL.md` — combined session-start +
finish-milestone checklist (the design that used to be a "TODO next session"
section here; removed now that it's implemented — see the skill file itself for
the procedure).
**M8: done, user confirmed working.** Exact plan spec: "theme pass,
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
packaged one, then reported the 5 issues captured in the punch list above
during that same testing pass.
**Older milestones (M1–M7):** moved to `docs/history.md` to keep this section
lean — see that file for the full narrative and design decisions behind each.

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
- **Mutating `FavoritesModel` (add/remove) while Favorites-mode polling is
  actively running desyncs `targetIndex` correlation** until polling is
  restarted (stop/start, or toggle modes and back) — `buildPollTargets()` is
  only called again at `startPollingFavorites()` time, not automatically on
  every model mutation. Flagged during M6c, not fixed (matches this project's
  existing "flag deferred gaps explicitly" convention).
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
