# Post-v1 roadmap

Not designed in detail, no impact on the current architecture. See plan section
"Post-v1 Roadmap".

- **Save/Load session** — project file (connection + Normal-view selection +
  Favorites, as JSON), saved/loaded by name so the user can pick one at startup and
  "get started." Reuses the tag-import JSON infra already built for M6a. When built,
  this is also the natural point to add a proper **File/Tools/Help menu bar** (File:
  New/Open/Save/Save As Session, Recent Sessions; Tools: Import Tag Database; Help:
  About/docs).
- **Connection health/stats** — round-trip time and error-rate tracking as a simple
  status indicator, natural extension of the comms log.
- **Slave/simulator mode built into the app** — valuable for open-source dogfooding
  without hardware. Not needed while pymodbus covers dev-testing.
- **Code signing / notarization** — belongs in the eventual OSS-readiness pass (plan
  Decision 11) once the app is ready to distribute publicly.
- **Multi-connection / MDI** — v1 is explicitly single-connection; multiple
  simultaneous connection windows (matching the original Modbus Poll) is a real
  future extension, not designed now.
