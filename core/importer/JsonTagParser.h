#pragma once

#include "importer/ITagFileParser.h"

namespace ModbusViewer::Core {

// Root must be a JSON array of objects; keys match CsvTagParser's column names
// (case-insensitive: label, description, registerType, address, format, byteOrder,
// scale, offset, unit), values may be JSON strings or numbers. See TagRowParser for
// the shared field-validation rules and defaults.
class JsonTagParser : public ITagFileParser
{
public:
    TagParseResult parse(QIODevice &device) const override;
};

} // namespace ModbusViewer::Core
