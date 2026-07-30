#pragma once

#include <QList>
#include <QString>
#include <QtGlobal>

namespace ModbusViewer::Core {

// Per-register/per-tag display format. The first four are single-register (16-bit);
// the last three are register-pair (32-bit), see registerSpanFor().
enum class DisplayFormat {
    SignedDecimal,
    UnsignedDecimal,
    Hex,
    Binary,
    Float32,
    Int32Signed,
    Int32Unsigned,
};

// How the four bytes of a 32-bit value are distributed across the two 16-bit
// registers that carry it. ABCD is standard big-endian (register0 = high 16 bits);
// the other three are the byte/word-swapped permutations real devices use. Only
// meaningful for the register-pair formats.
enum class ByteOrder { ABCD, BADC, CDAB, DCBA };

struct FormatSettings
{
    DisplayFormat format = DisplayFormat::UnsignedDecimal;
    ByteOrder byteOrder = ByteOrder::ABCD;
    double scale = 1.0;
    double offset = 0.0;
    QString unit;
};

// 1 for the single-register formats, 2 for the register-pair formats.
int registerSpanFor(DisplayFormat format);

// rawRegisters.size() must equal registerSpanFor(settings.format). Scale/offset/unit
// apply only to the decimal-ish formats (Signed/Unsigned/Float32/Int32) -- Hex/Binary
// always show the raw bit pattern.
QString formatValue(const FormatSettings &settings, const QList<quint16> &rawRegisters);

// The bare numeric value formatValue() would otherwise stringify -- scale/offset
// applied for the decimal-ish formats, raw unsigned register value for Hex/Binary
// (scale/offset never apply there, same rule as formatValue()). For callers that
// need a plottable double rather than display text (e.g. FavoritesModel's
// sparkline history).
double numericValue(const FormatSettings &settings, const QList<quint16> &rawRegisters);

// The inverse of formatValue: parses a formatted (or plain numeric) string back into
// raw register(s) suitable for a Modbus write. Sets *ok to false and returns an empty
// list if text cannot be parsed for the given format.
QList<quint16> parseValue(const FormatSettings &settings, const QString &text, bool *ok);

} // namespace ModbusViewer::Core
