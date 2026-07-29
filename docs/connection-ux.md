# Connection UX

## Flow: Connection screen -> Main screen, not a modal dialog

`Main.qml` hosts a `StackView` opening on `ConnectionScreen.qml`, pushing
`MainScreen.qml` on successful connect (smooth transition). `ConnectionScreen.qml`
auto-scans serial ports on load via `SerialPortListModel` (wraps
`QSerialPortInfo::availablePorts()`), plus a manual "Rescan" for hot-plugged devices —
no background device-change polling in v1.

`ConnectionController` exposes `ConnectionState`
(Disconnected/Connecting/Connected/ConnectionLost/Failed), driving the screen
transition and an inline error banner on failure — stays on the connection screen so
the user can fix a field and retry, no dead-end navigation. "Disconnect" in
`MainScreen`'s toolbar pops back to `ConnectionScreen`. `HandshakeAnimation.qml` plays
while `ConnectionState == Connecting`. See plan Decision 19.

## Field defaults

- **RTU**: Baud Rate **115200** (dropdown of common presets, editable), Data Bits 8,
  Parity **Even** (the Modbus RTU spec's own default, overridable — many real devices
  actually run None), Stop Bits 1, Flow Control None, Unit/Slave ID 1.
- **TCP**: Host, Port (default 502), Unit ID.
- **Both**: Timeout/Retry (Decision 18), `reconnectIntervalMs` (Decision 21).

## Disconnection handling: keep last data, watermark, auto-reconnect

A **hard transport loss** (TCP socket closed, serial port errored) — not a soft
per-row poll timeout, which stays a row-level stale/error indicator — sets
`ConnectionState` to a distinct `ConnectionLost` (vs. user-initiated `Disconnected`,
which pops back to `ConnectionScreen`).

`ConnectionLost` does **not** navigate away from `MainScreen` and does **not** clear
`RegisterTableModel`/`FavoritesModel` — last-known values stay visible, dimmed under
`DisconnectedWatermark.qml` (full-screen overlay). `PollEngine` stops issuing new
requests while lost. `ConnectionController` auto-retries on a `QTimer` at the
user-configured `reconnectIntervalMs` (default 5000ms); on success, the watermark
clears and `PollEngine` resumes against the *same* `PollTargetSet` — no
reconfiguration needed. See plan Decision 21.
