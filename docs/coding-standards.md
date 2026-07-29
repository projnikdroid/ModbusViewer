# Coding standards

Pragmatic Clean Code (Robert C. Martin) + Karpathy simplicity bias. Governs how every
milestone's code is written — not a module, a discipline. See plan Decision 20.

## From Clean Code

- Intention-revealing names. No cryptic abbreviations outside accepted domain
  vocabulary (PDU/CRC/ADU/MBAP are fine; ad-hoc shorthand isn't).
- Small functions that do one thing.
- Minimal argument lists — favor option structs over long parameter lists
  (`CoalescingOptions`, `PollTarget`, etc.).
- No boolean flag arguments that silently branch behavior — prefer an enum or a
  separate function.
- Comments only for non-obvious *why*, never restating *what*.
- Result-style returns (`{data, errors}`) over scattered error codes.

## From Karpathy's engineering philosophy

- Build a minimal working skeleton first, verify end-to-end, add complexity one
  verified piece at a time — never multiple unverified changes at once. This is why
  the milestones are ordered the way they are: M1-M3 prove one real end-to-end TCP
  read before M5a-M6d add coalescing/favorites/search complexity.
- Code should be readable top-to-bottom without chasing indirection.
- **The explicit tie-breaker**: every abstraction (interface, registry, base class)
  must justify itself against an actual current requirement, not a hypothetical
  future one. `IFunctionCodeHandler` is justified — custom function codes are a real
  stated requirement. A similar interface added "just in case" would not be.

## TDD

Tests written before/alongside each unit of work, not after. `core/` pieces
(coalescer, parsers, protocol codec, formatter) are pure Qt Test/CTest with no QML
dependency. `RegisterFilterProxyModel` and the models are the exception needing a
headless `QCoreApplication` test harness. See plan Decision 10.
