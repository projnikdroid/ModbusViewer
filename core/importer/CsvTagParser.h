#pragma once

#include "importer/ITagFileParser.h"

namespace ModbusViewer::Core {

// Simple comma-split CSV, no quoted-field escaping -- a field containing a literal
// comma (e.g. a description) isn't supported in v1. Header row is required; column
// order is free and names are matched case-insensitively. Required columns: label,
// registerType, address. Optional: description, format, byteOrder, scale, offset,
// unit (see TagRowParser for defaults). A malformed row is skipped with an error
// appended, not fatal to the rest of the import.
class CsvTagParser : public ITagFileParser
{
public:
    TagParseResult parse(QIODevice &device) const override;
};

} // namespace ModbusViewer::Core
