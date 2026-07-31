# ModbusViewer — Progress

Single source of truth for where the project stands. Read this first in any new
session, then load only the specific `docs/*.md` topic file you need. The full
rationale behind every decision referenced here lives in the approved plan at
`C:\Users\projn\.claude\plans\zany-sleeping-elephant.md`.

## Current status

**Carried-over action items — still open, not part of this session's theme work:**
1. **V1.2.0 released (2026-07-30)**: tagged `v1.2.0`, project version bumped
   to 1.2.0 in `CMakeLists.txt`, packaged via the existing `windeployqt`
   recipe, verified standalone (same `PATH`-stripped check as prior
   releases), and published as a GitHub Release with the zip attached:
   https://github.com/projnikdroid/ModbusViewer/releases/tag/v1.2.0. Ships
   the M11-M11e theme work above, plus: a custom app icon (`app/icons/app.ico`,
   embedded both as the `.exe`'s own resource icon via `app/app.rc` and as
   the runtime window/taskbar icon via `QGuiApplication::setWindowIcon()`);
   a small credits watermark on `ConnectionScreen.qml` with a click-triggered
   "warp core" pulse animation (purely decorative, user's explicit request);
   and the README's first real screenshots (`docs/screenshots/`, replacing
   the "coming soon" placeholder) covering Normal mode and both Favorites
   views.
2. **V1.1.0 released (2026-07-30)**: tagged `v1.1.0`, project version bumped
   to 1.1.0 in `CMakeLists.txt`, packaged via the existing `windeployqt`
   recipe (`packaging/windows/README.md`), verified standalone (launched with
   `PATH` stripped to bare `C:\Windows\System32`, same check as M8), and
   published as a GitHub Release with the zip attached:
   https://github.com/projnikdroid/ModbusViewer/releases/tag/v1.1.0.
3. **GitHub community/OSS setup completed (2026-07-30)**: `SECURITY.md`
   (private vulnerability reporting also enabled via repo settings),
   `CODE_OF_CONDUCT.md` (Contributor Covenant 2.1), `.github/ISSUE_TEMPLATE/`
   (bug report + feature request forms) and `.github/PULL_REQUEST_TEMPLATE.md`
   — community health checklist now 100% (was 57%). Wiki populated with
   Home/Getting-Started/Modbus-Register-Types pages (the wiki's git repo only
   exists after a first page is created via the web UI — no API for that —
   so the user created `Home` manually and the other two pages were pushed
   via `git` to `https://github.com/projnikdroid/ModbusViewer.wiki.git`,
   same repo credential already used for the main repo).
4. **Check whether the merged CI+CodeQL workflow run actually passed** —
   `gh run view 30420317211 --repo projnikdroid/ModbusViewer` (or `gh run list`
   for whatever's latest on `main`) if this hasn't already been confirmed.
   Carried over from 2026-07-28, still not reverified — see the OSS-readiness
   entry in `docs/history.md` for the full saga if it needs revisiting.
   **Note**: item 6 below changed when CI runs going forward, so a fresh
   `gh run list` won't show one for ordinary commits anymore — this item is
   about that already-existing historical run, not a new one.
5. **Fixed (2026-07-30) — was open here as "not yet investigated": Normal
   mode's register table columns didn't reflow correctly on window resize.**
   Investigated and fixed as part of this session's M11 GUI-verification pass
   (real repro, not the original guess) — see the M11 narrative below for the
   actual root cause and fix (a `TextField` minimum-width floor plus missing
   `clip: true`, not the originally-suspected `columnWidthProvider`).
6. **CI trigger narrowed (2026-07-30), per explicit user request.**
   `.github/workflows/ci.yml` no longer runs on every push to `main` or on
   every pull request — it was pure noise for a solo repo. Now runs only on
   a `v*` tag push (i.e. an actual release, same as `v1.1.0`), manually via
   `workflow_dispatch`, or the pre-existing weekly CodeQL sweep (unchanged).
   **Known tradeoff, flagged but not resolved**: ordinary commits and PRs no
   longer get automatic CI validation. Fine for a solo maintainer; revisit
   (add `pull_request:` back, at least) if this project ever takes outside
   contributions, so contributor PRs get tested automatically again.

**M11-M11e: Two selectable app themes, "Glass HUD" and "Signal Console" —
done (2026-07-30), user GUI-verified after a real bug-finding pass.** Full rationale
and design in the approved plan at
`C:\Users\projn\.claude\plans\before-moving-ahead-can-clever-acorn.md`.
Genesis: PROGRESS.md's own "plan a futuristic redesign" flag led to a
3-concept survey Artifact (Glass HUD / Studio Minimal / Signal Console)
reacting to the real `ConnectionScreen.qml` fields; user picked Glass HUD +
Signal Console as **two user-selectable themes** (not one fixed redesign),
confirmed via clarifying questions: full custom-skinned controls (not just a
color swap), persisted across restarts via `QSettings`, picker on both
`ConnectionScreen.qml` and `MainScreen.qml` — plus a fourth constraint added
mid-plan: the frontend (QML/theme definitions) must stay independent of the
backend (C++/persistence), so a future theme is a QML-only addition.

Exploration before planning confirmed: `Theme.qml` was read via ~180 plain
`Theme.xxx` bindings across 8 files (all `readonly`, no runtime switching);
no custom control delegates existed anywhere (`Button`/`ComboBox`/`SpinBox`/
`TextField`/`TabBar`/`TabButton`/`Switch`/`CheckBox` all rendered via
QtQuick Controls' default "Basic" style); zero `QSettings` usage anywhere in
the app (this is the first persisted setting); no `Qt6::QuickEffects`/blur
module linked.

- **M11** (`app-lib/ThemeSettings.h/.cpp`): new class, `QString themeId`
  `Q_PROPERTY` — deliberately **no enum, no validation** against known theme
  names, just an opaque persisted string (the frontend/backend decoupling
  requirement). `QSettings`, `IniFormat` explicitly, key `"ui/themeId"`,
  loaded synchronously in the constructor (available before `Theme.qml`'s
  first read, no flash-of-default-theme). Constructor takes an optional
  settings-file-path override as the test injection seam (`QTemporaryDir` in
  `test_theme_settings.cpp`, never touching the real config). `main.cpp`
  gained `QCoreApplication::setOrganizationName`/`setApplicationName` +
  `QSettings::setDefaultFormat(IniFormat)` — required for a
  default-constructed `QSettings()` to resolve anywhere at all; neither name
  was previously set. 5 new tests (TDD, written first): default-empty,
  persist-round-trip, empty-string round-trip, arbitrary-unrecognized-id
  round-trip (proves the backend really doesn't validate), guard-assign-emit
  signal behavior. 23/23 suites green (up from 22).
- **M11a** (`app/qml/theme/Theme.qml`): restructured internals, not its
  external property surface — a `palettes` JS object (`glassHud`,
  `signalConsole`) plus `active: palettes[ThemeSettings.themeId] ||
  palettes.glassHud` (a real binding read, not a `Q_INVOKABLE` call, so it
  re-evaluates automatically) and `availableThemes` (the picker's data
  source, so adding a theme later never touches the picker or the C++ side).
  Every existing top-level token becomes `readonly property X: active.X`, so
  all ~180 pre-existing bindings keep working unchanged. New tokens:
  `fontFamilyMono`, `accentGradientStops` (2-stop cyan→violet for Glass HUD,
  empty for Signal Console's flat amber), `blurEnabled`/
  `gridBackgroundEnabled`.
- **M11b** (`app/qml/components/ThemeBackdrop.qml`): decorative per-theme
  background, reusing the exact `Canvas` technique already in this codebase
  (M10c's sparkline) — Glass HUD's radial wash via Canvas 2D's
  `createRadialGradient()` (QML's own `Rectangle.gradient` is linear-only),
  Signal Console's faint amber grid via plain hairline strokes. Painted once
  per resize/theme-change, not per frame. Placed behind both screens' root
  `Rectangle`.
- **M11c** (`app/qml/controls/Themed*.qml`, 9 files): one delegate per
  control type actually used bare in the app —
  `Button`/`ComboBox`/`SpinBox`/`TextField`/`TabBar`/`TabButton`/`Switch`/
  `CheckBox` (confirmed via grep, exactly these 7) plus `Popup` (found during
  implementation, not the original grep list — `FormatPicker.qml`'s root is
  a `Popup`, and `MainScreen.qml`'s "Add From Tag..." popup too; leaving
  either unthemed would put a default-white panel in an otherwise fully
  skinned screen). Each delegate customizes `background`/`contentItem`/
  `indicator` against the same reactive `Theme.*` tokens, so **one delegate
  set serves both themes**. `ThemedComboBox`'s `contentItem` is a `TextField`
  (not a plain `Text`) specifically so the one editable combo in the app
  (`RtuSettingsPanel`'s baud-rate picker) keeps working. All bare control
  instantiations across the 6 consumer files swapped in mechanically
  (anchored regex substitution on the type name only, verified zero
  unintended matches before applying).  **Self-review catch**: swapping
  `TextField`'s themed background from Basic-style's default light panel to
  `Theme.surface` (dark in both themes) turned a pre-existing `color:
  registerModel.stale ? Theme.warning : "black"` literal-black hack (in the
  Normal-mode and Favorites value fields, dating to when the field's own
  background was light) into invisible black-on-dark text — fixed to
  `Theme.textPrimary` as part of this milestone, not left for GUI
  verification to catch.
- **M11d** (hero-element polish): `ThemedButton` gained an opt-in `accented`
  property (default `false` — a plain themed button unless explicitly set)
  driving the gradient-vs-flat accent background from `accentGradientStops`;
  set `true` only on the Connect button and "Reconnect Now". Connection
  screen's title got a thin gradient/flat accent rule underneath (QML
  `Text` can't be gradient-filled without an effects module this project
  deliberately isn't adding — the gradient lives on a native
  `Rectangle.gradient` bar instead). `Theme.fontFamilyMono` applied to the
  Normal/Favorites value fields and the Favorites card's big value text.
  Connection-screen field labels (`Host`/`Port`/`Baud Rate`/etc., 11 total
  across `ConnectionScreen.qml`/`TcpSettingsPanel.qml`/`RtuSettingsPanel.qml`
  — the one screen the approved mockup actually showed this way) gained
  uppercase + letter-spacing, scoped surgically rather than a blanket
  relabel of every `Label` in the app.
- **M11e** (picker UI): a `ThemedComboBox` on both screens, `model:
  Theme.availableThemes` / `textRole: "label"` / `valueRole: "id"` —
  the picker itself never hardcodes theme names, so a future third theme
  needs no picker change either. `MainScreen.qml`: appended to the existing
  toolbar, after the trailing fill-width spacer. `ConnectionScreen.qml`: new
  anchored top-right corner control (that screen has no existing toolbar
  row).

23/23 suites green after every milestone. Build clean + headless launch
check (no QML errors on the Debug console) after M11a/M11b/M11c/M11e — M11c
in particular given the file count (6 consumer files + 9 new delegates).

**GUI-verification pass (2026-07-30) found and fixed several real bugs the
headless/automated checks couldn't catch** — exactly the class of problem
this project's "GUI verification is the user's job" convention exists to
catch:

- **Dropdown/popup text invisible** (Theme picker, register-type combo,
  FormatPicker's Display Format combo): the delegate's hand-rolled
  `modelData[textRole]` lookup silently evaluated to `undefined` for every
  row — no QML warning, just blank text. Replaced with `ComboBox.textAt(index)`,
  the built-in, textRole-aware lookup, via a new shared `ThemedItemDelegate`
  (also fixes "Add From Tag..."'s search-result list, which had never been
  swapped off Basic style's default `ItemDelegate` at all).
- **Glass HUD's popups/dropdowns unreadable**: `Theme.surface`'s intentional
  translucency (the in-page "glass panel" look) washed out to near-white in
  a floating `Popup`'s own overlay layer, which has no reliable dark backdrop
  behind it. New opaque `Theme.surfaceOpaque` token, used only by floating
  overlays (`ThemedComboBox`'s popup, `ThemedPopup`).
- **Every SpinBox in the app became read-only**: `ThemedSpinBox`'s
  `contentItem` gated typing on `control.editable`, which nothing in this
  app has ever set — removed, since every SpinBox here is meant to be typed
  into directly.
- **Popups opened pinned to the window's top-left corner**: `ThemedPopup`
  never positioned itself. Added `x`/`y` centering *bindings* (not one-time),
  so it also re-centers if the window is resized/maximized afterward.
- **ConnectionScreen's theme picker didn't respond to clicks**: the
  `Flickable` below it in the file was declared after it, so it painted (and
  captured input) on top, swallowing clicks meant for the picker. Fixed with
  `z: 1` on the picker.
- **Real crash**: switching a Favorites entry's display format between
  register spans of different width (e.g. Decimal → Float32) resets that
  entry's raw data to the new size immediately, but a poll response already
  in flight from *before* the change could still arrive sized for the *old*
  span. Applying it unconditionally desynced the stored data from what the
  display code's `Q_ASSERT` expects for the new format, aborting the whole
  process — this is also what the momentary `1.88079e-37`-style garbage
  value was (a stray register misread as a float, right before the mismatch
  crashed). Fixed two ways: a format change now retargets the active poll
  immediately (`FormatPicker.formatApplied` signal, same mechanism already
  used for add/remove), and `FavoritesModel::applyRegisterUpdate()` now
  discards any response whose size doesn't match the entry's current
  `registerSpan()` rather than applying it. **Verified TDD-style**: new test
  `applyRegisterUpdateIgnoresAResponseSizedForTheFormatBeforeALiveFormatChange`
  in `test_favorites_model.cpp`, confirmed via `ctest -R test_favorites_model`
  to genuinely abort the process (`0xc0000602`) with the guard disabled,
  then confirmed clean with it restored. 23/23 suites green (up from 22).
- **The originally-flagged "columns don't reflow on resize" bug, actually
  root-caused this time**: Basic style's default `TextField` implicit width
  is wider than the row can always afford once its column narrows, and
  nothing was clipping the overflow — it rendered *past* the cell instead of
  shrinking or being cut off, invisible until the window was widened enough
  to reach it. Fixed with an explicit smaller `Layout.minimumWidth` on the
  value fields plus `clip: true` on both the Normal-mode table row and the
  Favorites list row.
- **Toolbar didn't fit narrower windows**: the main toolbar was a single
  non-wrapping `RowLayout` — anything past the available width (Search,
  Import Tags, Hide Log, Disconnect) just ran off the edge with no scrollbar
  and no way back short of widening the window. Switched to a `Flow`, which
  wraps onto a second line instead.
- **Value fields now size to their content** (a small min/max-bounded
  `Layout.preferredWidth` from `contentWidth`) instead of stretching to fill
  the whole row, per direct user feedback once resizing was fixed and the
  full-width stretch became obviously excessive. The ⚙/✕ action buttons
  stay clustered next to the value at the row's right edge via an
  unconditional filler `Item`, matching the layout Favorites already used
  for its bit-type rows.

**Known, explicitly deferred (not a blocker)**: the "Flash on update"
`ThemedCheckBox` shows what the user describes as two overlapping
check-mark-shaped visuals when checked/hovered. Investigated two rounds
(suspected Basic-style default background chrome, then suspected `"✓"`
glyph/emoji-presentation font fallback — replaced with a hand-drawn `Canvas`
stroke) without resolving it; still visible with the drawn version too, at
different apparent sizes/colors, which rules out both font-fallback theories.
Most likely explanation left: the indicator's own two-part composition (a
filled box *containing* a separate check mark) reading as "two shapes" at
18px, not an actual duplicate element — grepping confirms only one
`ThemedCheckBox` instance and one checkmark in its code. **User decided this
isn't worth pursuing further** — left as-is, flagged in "Known rough edges"
below if revisited later.

**Older milestones (M1–M10c, the post-M10c "values not updating"
investigations and correlation-id bugfix, the post-M8 punch list, and the
OSS-readiness pass):** moved to `docs/history.md` to keep this section lean —
see that file for the full narrative and design decisions behind each.

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
| M11 | ThemeSettings persistence backend + tests | **Done** |
| M11a | Theme.qml palette-map restructure (Glass HUD / Signal Console) | **Done**, user GUI-verified |
| M11b | Decorative background Canvases | **Done**, user GUI-verified |
| M11c | Themed control delegates (10 files) + swap-in | **Done**, user GUI-verified |
| M11d | Hero-element polish (gradient accents, mono data fields, tracked labels) | **Done**, user GUI-verified |
| M11e | Theme picker UI on both screens | **Done**, user GUI-verified |

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
- **`ThemedCheckBox` ("Flash on update") shows what looks like two
  overlapping check-mark visuals when checked/hovered**, per direct user
  report (2026-07-30). Two rounds of investigation (Basic-style default
  background chrome, then `"✓"` glyph/emoji font-fallback rendering — both
  ruled out, including after replacing the glyph with a hand-drawn `Canvas`
  stroke) didn't resolve it. Only one `ThemedCheckBox` instance and one
  checkmark element exist in the code (confirmed via grep) — likely the
  indicator's own box+mark composition reading as "two shapes" at 18px
  rather than an actual duplicate. **User explicitly decided not to pursue
  this further** — cosmetic only, revisit only if it bothers someone later.

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
