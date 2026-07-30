# Security Policy

## Supported versions

ModbusViewer is a desktop application, not a hosted service — there's no
"latest supported version" split the way a library or server project would
have. Security fixes land on `main` and are included in the next tagged
release. Always use the
[latest release](https://github.com/projnikdroid/ModbusViewer/releases) if
you're concerned about a known issue.

## Reporting a vulnerability

Please **do not** open a public GitHub issue for security vulnerabilities.

Instead, use GitHub's private vulnerability reporting:

1. Go to the [Security tab](https://github.com/projnikdroid/ModbusViewer/security).
2. Click **"Report a vulnerability"**.

This opens a private advisory visible only to the maintainer, so the issue
can be discussed and fixed before any public disclosure.

You should expect an initial response within a few days. This is a
single-maintainer open-source project, so please be patient — there's no SLA,
but reports are taken seriously and will be addressed as soon as possible.

## Scope

Relevant reports include (but aren't limited to):

- Memory-safety issues in the Modbus protocol codec or framing
  (`core/modbus/`) — this is the code path that parses untrusted bytes
  arriving from the network/serial line.
- Anything that could let a malicious or misbehaving Modbus slave device
  crash the application or corrupt memory via a crafted response.
- Credential/secret handling issues (though v1 has no credential storage —
  connections are host/port or serial-port settings only).

Out of scope: the bundled dev-time simulator
(`tools/pymodbus_simulator.py`) is a testing convenience, not part of the
shipped application, and isn't meant to be exposed to untrusted networks.
