# Contributing to ModbusViewer

## Getting set up

See [`README.md`](README.md) for build, test, and dev-simulator instructions.

## Before you start

Read [`docs/architecture.md`](docs/architecture.md) for the overall design, and
whichever other `docs/*.md` topic file is relevant to what you're changing
(`protocol.md`, `performance.md`, `favorites-search-tags.md`, `connection-ux.md`).
[`PROGRESS.md`](PROGRESS.md) has the current state of the project, known rough
edges, and hard-won gotchas worth reading before touching related code.

## Coding standards

Full detail in [`docs/coding-standards.md`](docs/coding-standards.md); the short
version:

- **Tests are written before or alongside the implementation, not after.** `core/`
  code (protocol codec, coalescer, parsers, formatter) is plain Qt Test/CTest with no
  QML dependency. Classes exposed to QML get a headless `QCoreApplication`-based test
  harness instead.
- **Every abstraction must justify itself against an actual current requirement**,
  not a hypothetical future one. Prefer three similar lines over a premature
  interface.
- Intention-revealing names; small functions that do one thing; option structs over
  long parameter lists; no boolean flags that silently branch behavior; comments only
  for non-obvious *why*, never restating *what*.
- Build a minimal working piece end-to-end, verify it, then add complexity —
  not several unverified changes at once.

## Running the tests

```bash
cd build
ctest --output-on-failure
```

All suites should pass before opening a PR. If you're changing `core/` or
`app-lib/` behavior, add or update a test alongside the change — see the existing
`tests/*.cpp` files for the conventions used (`FakeTransport` for protocol/transport
-layer tests, a real loopback `QTcpServer` for TCP connection-lifecycle tests).

## Submitting a change

1. Open an issue first for anything beyond a small fix, so the approach can be
   discussed before you invest time in it.
2. Keep PRs focused — one logical change per PR.
3. Make sure `ctest` passes locally before opening the PR; CI will also run it.
4. Describe *why* the change is needed, not just what it does — the commit history
   and PR descriptions are the record of design decisions that `docs/` doesn't
   capture inline.

## Reporting bugs

Include: what you did, what you expected, what happened instead, and whether it's
TCP or RTU (RTU has only been tested against a `FakeTransport` double so far — see
`docs/history.md`'s M4 entry — so real-hardware RTU reports are especially useful).
