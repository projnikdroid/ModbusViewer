# Architecture

## Stack

C++ / Qt 6.5+ LTS / Qt Quick (QML), CMake build. Cross-platform: Windows + macOS.
Qt modules: Core, Gui, Qml, Quick, QuickControls2, Network, SerialPort, Test.

CMake, not Meson: Qt6's `qt_add_qml_module` (auto qmldir generation, compile-time QML
type checking, Quick Compiler bytecode) is CMake-specific tooling with no Meson
equivalent. See plan Decision 14.

## Module layout

- **`core/`** — Qt-UI-independent, no QML dependency, fully unit-testable via Qt
  Test/CTest. Modbus protocol codec/framing/CRC16, transport abstraction, value
  formatting, tag/register-definition model, CSV/JSON parsers, polling/coalescing
  engine.
- **`app-lib/`** — Qt/QML-facing glue: `QAbstractItemModel`/`QObject` classes
  (`QML_ELEMENT`/`QML_SINGLETON`) bridging `core/` to QML.
- **`app/qml/`** — UI: screens (`ConnectionScreen`, `MainScreen`), views (Normal,
  Favorites), components, theme.
- **`tests/`** — Qt Test + CTest, written before/alongside implementation (TDD).

## Modbus protocol engine: hand-rolled, no libmodbus

libmodbus is blocking/synchronous and would fight the async Qt event-loop model. The
protocol itself (MBAP header, RTU CRC16, 8 function codes) is compact enough to
implement directly, avoids a second dependency to package/notarize on both platforms,
and gives full control over raw bytes for the communication log panel. Extensibility
for future custom function codes: a `QMap<quint8, IFunctionCodeHandler*>` registry in
`ModbusTransactionManager`, not a hardcoded switch. See plan Decision 2.

## Full class/file map

See the "Project Structure" tree in the approved plan
(`C:\Users\projn\.claude\plans\zany-sleeping-elephant.md`) for the complete file
layout — not duplicated here since it changes as milestones land; `PROGRESS.md`
tracks what's actually been built.
