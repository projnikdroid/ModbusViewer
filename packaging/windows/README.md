# Windows packaging

`dist/` is a self-contained, ready-to-run copy of ModbusViewer — copy the whole
folder anywhere and run `ModbusViewer.exe`. No Qt or MinGW install is required
on the target machine; every dependency (Qt DLLs, the MinGW runtime, QML
modules, plugins) is bundled alongside the executable.

## How it was built

```powershell
$env:Path = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\Ninja;C:\Qt\6.11.1\mingw_64\bin;" + $env:Path
cmake -S . -B build-release -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64" -DCMAKE_BUILD_TYPE=Release
cmake --build build-release

mkdir packaging\windows\dist
copy build-release\ModbusViewer.exe packaging\windows\dist\
windeployqt --qmldir app\qml packaging\windows\dist\ModbusViewer.exe
```

`--qmldir app\qml` is needed so `windeployqt` can scan the actual QML sources
for which QML modules are used (`QtQuick.Dialogs`/`Qt.labs.folderlistmodel`
etc.) — without it, some QML-only dependencies can be missed since the app
loads QML from the `qt_add_qml_module`-generated resources, not a plain
directory `windeployqt` would otherwise infer from the executable's imports.

## Verifying it's actually standalone

Confirmed by launching `dist\ModbusViewer.exe` with `PATH` stripped down to
just `C:\Windows\System32;C:\Windows` (no `C:\Qt\...`, no MinGW) — it started
and stayed running normally, proving it doesn't depend on anything from the dev
environment.

## macOS

Not built here — this is a Windows-only development machine, so `macdeployqt`
hasn't been exercised. The same pattern applies: build Release, then
`macdeployqt ModbusViewer.app`.
