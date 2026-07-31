# ModbusViewer

Cross-platform (Windows + macOS) Qt6/QML clone of Modbus Poll — a Modbus TCP/RTU
master tool. C++20, CMake, Qt 6.11.1 (MinGW kit, self-contained, no MSVC).

## Start every session here

This is codified as the `modbusviewer-workflow` project skill
(`.claude/skills/modbusviewer-workflow/SKILL.md`) — invoke it at session start
(including a bare "continue") and again when a milestone is done, rather than
re-deriving the steps below by hand.

1. **Read `PROGRESS.md` first** — current milestone, environment state, known rough
   edges, and hard-won gotchas. It is the single source of truth for "where things
   stand"; this file is not.
2. Load only the specific `docs/*.md` topic file relevant to the work at hand
   (`architecture.md`, `protocol.md`, `performance.md`, `favorites-search-tags.md`,
   `connection-ux.md`, `coding-standards.md`, `roadmap.md`).
3. Full rationale behind every architecture decision lives in the approved plan:
   `C:\Users\projn\.claude\plans\zany-sleeping-elephant.md`.
4. `docs/history.md` has the full narrative for every *completed* milestone older
   than the most recent one or two (moved there to keep `PROGRESS.md` lean). Not
   part of routine session start — only open it if you need the *why* behind a
   past decision that isn't in `PROGRESS.md`'s "Current status" or the "Notes from
   implementation"/gotchas sections.

## Build & test

```powershell
$env:Path = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\Ninja;C:\Qt\6.11.1\mingw_64\bin;" + $env:Path
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64" -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cd build; ctest --output-on-failure
```

Executable: `build/ModbusViewer.exe`. First run of a freshly built `.exe` is slow
(~10-25s, Windows Defender scanning it) — not a code issue, reruns are fast.

To run it, only the Qt runtime DLLs need to be on `PATH` (not the full MinGW/Ninja
build toolchain):

```powershell
$env:Path = "C:\Qt\6.11.1\mingw_64\bin;" + $env:Path
& "C:\Users\projn\OneDrive\Desktop\Claude\Projects\ModbusViewer\build\ModbusViewer.exe"
```

## Dev-time simulator

`python` is not on this machine's `PATH` — use the full interpreter path:

```powershell
Set-Location "C:\Users\projn\OneDrive\Desktop\Claude\Projects\ModbusViewer"
& "C:\Users\projn\AppData\Local\Python\bin\python.exe" tools\pymodbus_simulator.py --mode tcp --port 502
```

Pinned to `pymodbus==3.7.4` (see `tools/requirements.txt` for why — 3.8+ deprecated
the classic datastore API this simulator uses).

## Working method (non-negotiable, per the approved plan)

- **TDD**: the test is written before the implementation, every milestone, no
  exceptions.
- **Self-review before building/testing.** After writing an implementation unit,
  reread it against the test's expected values before invoking `cmake --build`/
  `ctest` — check sign handling, byte/word order, off-by-one, and boundary
  conditions by hand first. Catching a logic error by inspection is faster than a
  build-fail-fix-rebuild loop; the actual build/test run still happens afterward as
  confirmation, it's just not the first line of defense.
- **Clean Code + Karpathy simplicity** (`docs/coding-standards.md`): every
  abstraction must justify itself against an actual *current* requirement, not a
  hypothetical future one.
- Work proceeds in small, independently verifiable milestones — see `PROGRESS.md`'s
  milestone table for the full list and current position.
- **GUI verification is the user's job, not the agent's.** The agent builds and runs
  `ctest`; the user runs the app and reports back (screenshots or description).
  Driving the GUI from the agent side (launch/screenshot/synthetic clicks via
  PowerShell) works but is far too slow to be worth it — confirmed during M1.

## Git commit conventions (non-negotiable, per explicit user request 2026-07-30)

- **Never add a `Co-Authored-By: Claude ...` trailer to commit messages in
  this repo.** This overrides the harness's own default git-commit
  instructions for this specific repository — commits should read as the
  user's own work, not co-authored. This applies to every commit going
  forward, not just the current session.

## Context-efficiency practices (apply every session, not just when reminded)

- **Filter tool output before it enters context, not after.** Pipe build output
  through `grep -iE "error|FAILED" -A6` rather than reading the full log; a build
  log can be thousands of lines when the signal is a dozen. Same principle for any
  verbose command.
- **Don't re-verify what a tool already confirmed.** Don't re-read a file
  immediately after Edit/Write — a successful tool call already means it applied.
  Don't re-run a check with no state change since the last run.
- **Offload research, not decisions, to subagents.** Multi-file exploration/search
  belongs in an Explore/general-purpose agent (comes back summarized); keep the main
  thread for synthesis, implementation, and judgment calls.
- **Design before implementing on anything non-trivial.** Use plan mode for new
  features or architecture changes. Churn from re-deciding mid-implementation costs
  far more context than getting the design agreed first.
- **Background long-running commands (builds, test suites) and wait for the
  notification** — never poll a background task with manual `sleep` loops.
- **Batch independent tool calls in one message** rather than one-at-a-time
  round-trips.
- **Prefer starting a fresh session with state saved here/in `PROGRESS.md` over
  extending one long-running session.** A multi-hour single thread accumulates
  context that this file + `PROGRESS.md` make unnecessary to replay.

## Recurring gotchas (full detail and dates in `PROGRESS.md`)

- **QML singletons**: `pragma Singleton` in the `.qml` file is not enough with
  `qt_add_qml_module`. Also needs
  `set_source_files_properties(path/to/File.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)`
  in CMake, or it silently registers as a plain type — bindings resolve to nothing,
  **with no QML warning at all**. Verify via the generated `qmldir` containing
  `singleton Foo ...`, not by reading the `.qml` source.
- Any C++ method QML calls needs `Q_INVOKABLE` — `public` alone is not enough. A
  missing one throws a TypeError in the handler that silently kills everything after
  it in that function.
- `WIN32_EXECUTABLE TRUE` hides QML runtime errors (no console). This project sets
  it to `$<NOT:$<CONFIG:Debug>>` so Debug builds keep a console; don't remove that.
- `app-lib/` sources are shared via `MODBUSVIEWER_APPLIB_SOURCES`/
  `MODBUSVIEWER_APPLIB_INCLUDE_DIRS` (top-level `CMakeLists.txt`) so both the app
  target and `tests/` compile them — use `modbusviewer_add_applib_test(...)` for any
  new test that touches `app-lib/` classes.
- A `QObject` member wrapping a Qt built-in (`QTcpSocket`, `QSerialPort`, etc.) must
  disconnect its own signals in its own destructor — the built-in can emit *during*
  its own destruction, reaching handlers on a partially-destroyed owner.
- Timeout/deadline timers must use `Qt::PreciseTimer`. The default coarse timer
  rounds to ~5% buckets and can fire before its own `QDeadlineTimer` reports expired.
- Never mutate a list while iterating it inside a slot that could be re-entered
  synchronously (e.g. a socket write failing inline mid-loop). Decide all mutations
  first, then act (write/emit) only after the loop is done.
- Qt Test macro choice matters: `QTEST_APPLESS_MAIN` for pure logic (no event loop),
  `QTEST_GUILESS_MAIN` when timers/signals need a real event loop but no GUI,
  `QTEST_MAIN` when a test needs real sockets (e.g. `QTcpServer`).
- **A `Q_INVOKABLE` call is not a QML binding dependency.** If a binding only calls
  a C++ method (doesn't read a `Q_PROPERTY`), QML has no way to know it should
  re-evaluate when something the method reads internally changes — the binding just
  goes stale silently. Force it by reading the actual property first (comma
  operator: `(SomeSingleton.someProperty, SomeSingleton.someMethod(...))`) so QML's
  dependency tracker sees the read. Bit us in M6 with `DisplaySettings.addressConvention`
  driving `toDisplayAddress()`/`toPduAddress()` calls in `MainScreen.qml`.
- **A `Q_INVOKABLE`/signal parameter of a QObject-derived pointer type needs the
  full class definition included, not just forward-declared** — moc generates
  metatype-registration code for every invokable's argument types, and that fails
  to compile (`static assertion failed: Meta Types must be fully defined`) against
  an incomplete type. Bit us in M6a: `TagDatabaseController::importCsv(..., TagDatabaseModel*)`
  needed `#include "models/TagDatabaseModel.h"`, not just `class TagDatabaseModel;`.
- **`QtQuick.Dialogs` (native file dialogs) isn't pulled in by `QuickControls2`.**
  It needs its own `find_package(Qt6 ... QuickDialogs2)` component and
  `target_link_libraries(... Qt6::QuickDialogs2)`, or the QML import silently fails
  to resolve at runtime. Added in M6a for `ImportTagFileDialog.qml`.
