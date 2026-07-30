#pragma once

#include <QString>

#include "format/ValueFormatter.h"
#include "model/RegisterType.h"

namespace ModbusViewer::Core {

// Where a tag came from: imported from a CSV/JSON register map, or hand-added by the
// user directly in the Favorites picker (M6c) with no backing metadata.
enum class TagSource { Imported, AdHoc };

// A "tag": static metadata describing what a register *is*, distinct from its live
// polled value. Reuses FormatSettings (M6) rather than duplicating
// format/byteOrder/scale/offset/unit as separate fields -- it's exactly that struct.
struct RegisterDefinition
{
    QString label;
    QString description;
    RegisterType registerType = RegisterType::HoldingRegister;
    int address = 0;
    FormatSettings format;
    TagSource source = TagSource::Imported;

    // How many consecutive registers this tag occupies. For bit types (Coil/
    // DiscreteInput) this is always 1, regardless of whatever FormatSettings
    // happens to carry -- scale/offset/byteOrder/DisplayFormat have no meaning for
    // a 1-bit value, so a stray Float32/Int32 format (e.g. a malformed import row)
    // must not be allowed to report a 2-register span. For word types it's implied
    // by the format (e.g. 2 for Float32) rather than stored/imported separately.
    int registerSpan() const
    {
        return isBitRegisterType(registerType) ? 1 : registerSpanFor(format.format);
    }
};

} // namespace ModbusViewer::Core
