#pragma once

#include <QMap>
#include <QString>

#include "model/RegisterDefinition.h"

namespace ModbusViewer::Core {

struct TagRowParseOutcome
{
    bool ok = false;
    RegisterDefinition tag;
    QString error;
};

// Validates and converts one tag's raw fields (already normalized to lowercase
// keys -- "label", "description", "registertype", "address", "format", "byteorder",
// "scale", "offset", "unit") into a RegisterDefinition. Shared by CsvTagParser and
// JsonTagParser so both file formats validate identically. Required: label,
// registertype, address. Everything else is optional and defaults to
// FormatSettings{}'s defaults (UnsignedDecimal/ABCD/scale 1/offset 0/no unit).
TagRowParseOutcome parseTagRow(const QMap<QString, QString> &fields);

} // namespace ModbusViewer::Core
