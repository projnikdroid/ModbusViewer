# Favorites, search, and the tag database

## Favorites vs Normal — one mode toggle, one PollEngine

Not two simultaneous poll loops. `PollModeController` (QML_SINGLETON) owns `mode`
(Normal/Favorites) and builds the active `PollTargetSet` accordingly:

- **Normal** derives targets from the selected address range (1:1 with visible rows).
- **Favorites** derives targets from `FavoritesModel` entries, each either backed by
  an imported `RegisterDefinition` (a "tag") or an ad-hoc one (raw address, no
  metadata — falls back to address-as-label, raw uint16 format).

Exactly one active target-set/engine at a time. See plan Decision 6.

## Register map (Tag) import

`RegisterDefinition` is static metadata (what a register *is*) distinct from live
polled values: label, description, register type, address, and a `FormatSettings`
(format/byteOrder/scale/offset/unit — reused from M6 rather than duplicated).
`registerSpan()` is derived from the format, not stored separately. `CsvTagParser`/
`JsonTagParser` (`core/importer/`) produce `QList<RegisterDefinition>` plus a
parallel error list — bad rows don't abort the whole import; both share one
field-validation path (`core/importer/TagRowParser.h/.cpp`) so the two file formats
can't drift apart. `TagDatabaseController` (QML_SINGLETON) drives import from QML,
calling `addTags()` directly on the `TagDatabaseModel*` passed to it (not by
relaying the parsed list back out through a signal — a plain C++ struct can't cross
the QML boundary that way without extra metatype registration). `TagDatabaseModel`
accumulates across multiple imports rather than replacing; it's what both the
Favorites picker and Normal-view tag lookup will draw from once M6c exists.

**File schema (M6a, designed during implementation — not specified in the original
plan):** header row required for CSV; column/key names case-insensitive, order-
independent. Required: `label`, `registerType` (`Coil`/`DiscreteInput`/
`HoldingRegister`/`InputRegister`), `address` (0-based PDU). Optional, with
defaults: `description` (empty), `format` (`UnsignedDecimal`; also `SignedDecimal`/
`Hex`/`Binary`/`Float32`/`Int32Signed`/`Int32Unsigned`), `byteOrder` (`ABCD`; also
`BADC`/`CDAB`/`DCBA`), `scale` (1), `offset` (0), `unit` (empty). CSV is plain
comma-split, **no quoted-field escaping** (a comma inside a description breaks the
row — known limitation, see `PROGRESS.md` rough edges). JSON is an array of objects
with the same keys, values as JSON strings or numbers.

## Search

`RegisterFilterProxyModel : QSortFilterProxyModel` (QML_ELEMENT) overrides
`filterAcceptsRow()` to match across label/description/address/unit — not a
single-role filter. Reused for both the tag-picker and the live table. The filter
predicate only inspects static columns, so it doesn't re-run on every poll-driven
value update, only on `filterText` or structural model changes — live cell updates
keep flowing through the filter uninterrupted. See plan Decision 8.
