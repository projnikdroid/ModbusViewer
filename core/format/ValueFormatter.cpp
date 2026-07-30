#include "ValueFormatter.h"

#include <bit>
#include <cmath>

namespace ModbusViewer::Core {

namespace {

// Assembles the two raw registers into a big-endian 32-bit word per byteOrder, i.e.
// value32 = (A<<24)|(B<<16)|(C<<8)|D where A/B/C/D are the standard big-endian bytes
// of the value and byteOrder says how they were distributed into r0/r1 on the wire.
quint32 packBytes(quint16 r0, quint16 r1, ByteOrder byteOrder)
{
    const quint8 h0 = quint8(r0 >> 8);
    const quint8 l0 = quint8(r0 & 0xFF);
    const quint8 h1 = quint8(r1 >> 8);
    const quint8 l1 = quint8(r1 & 0xFF);

    quint8 a = 0, b = 0, c = 0, d = 0;
    switch (byteOrder) {
    case ByteOrder::ABCD: a = h0; b = l0; c = h1; d = l1; break;
    case ByteOrder::BADC: a = l0; b = h0; c = l1; d = h1; break;
    case ByteOrder::CDAB: a = h1; b = l1; c = h0; d = l0; break;
    case ByteOrder::DCBA: a = l1; b = h1; c = l0; d = h0; break;
    }
    return (quint32(a) << 24) | (quint32(b) << 16) | (quint32(c) << 8) | quint32(d);
}

// The inverse of packBytes: splits a big-endian 32-bit word back into the two raw
// registers that would have carried it under byteOrder.
QList<quint16> unpackToRegisters(quint32 value32, ByteOrder byteOrder)
{
    const quint8 a = quint8((value32 >> 24) & 0xFF);
    const quint8 b = quint8((value32 >> 16) & 0xFF);
    const quint8 c = quint8((value32 >> 8) & 0xFF);
    const quint8 d = quint8(value32 & 0xFF);

    quint16 r0 = 0, r1 = 0;
    switch (byteOrder) {
    case ByteOrder::ABCD: r0 = (quint16(a) << 8) | b; r1 = (quint16(c) << 8) | d; break;
    case ByteOrder::BADC: r0 = (quint16(b) << 8) | a; r1 = (quint16(d) << 8) | c; break;
    case ByteOrder::CDAB: r0 = (quint16(c) << 8) | d; r1 = (quint16(a) << 8) | b; break;
    case ByteOrder::DCBA: r0 = (quint16(d) << 8) | c; r1 = (quint16(b) << 8) | a; break;
    }
    return {r0, r1};
}

bool isDefaultScaling(const FormatSettings &settings)
{
    return qFuzzyCompare(settings.scale, 1.0) && qFuzzyIsNull(settings.offset);
}

QString withUnit(const QString &numericText, const FormatSettings &settings)
{
    if (settings.unit.isEmpty())
        return numericText;
    return numericText + QLatin1Char(' ') + settings.unit;
}

} // namespace

int registerSpanFor(DisplayFormat format)
{
    switch (format) {
    case DisplayFormat::Float32:
    case DisplayFormat::Int32Signed:
    case DisplayFormat::Int32Unsigned:
        return 2;
    default:
        return 1;
    }
}

QString formatValue(const FormatSettings &settings, const QList<quint16> &rawRegisters)
{
    Q_ASSERT(rawRegisters.size() == registerSpanFor(settings.format));

    switch (settings.format) {
    case DisplayFormat::Hex:
        return QStringLiteral("0x")
            + QString::number(int(rawRegisters.first()), 16).toUpper().rightJustified(4, QLatin1Char('0'));
    case DisplayFormat::Binary:
        return QString::number(int(rawRegisters.first()), 2).rightJustified(16, QLatin1Char('0'));
    default:
        break;
    }

    const bool defaultScaling = isDefaultScaling(settings);
    QString numericText;

    switch (settings.format) {
    case DisplayFormat::SignedDecimal: {
        const qint16 value = qint16(rawRegisters.first());
        numericText = defaultScaling ? QString::number(value) : QString::number(numericValue(settings, rawRegisters));
        break;
    }
    case DisplayFormat::UnsignedDecimal: {
        const quint16 value = rawRegisters.first();
        numericText = defaultScaling ? QString::number(value) : QString::number(numericValue(settings, rawRegisters));
        break;
    }
    case DisplayFormat::Float32:
        numericText = QString::number(numericValue(settings, rawRegisters));
        break;
    case DisplayFormat::Int32Signed: {
        const qint32 value = qint32(packBytes(rawRegisters.at(0), rawRegisters.at(1), settings.byteOrder));
        numericText = defaultScaling ? QString::number(qint64(value))
                                      : QString::number(numericValue(settings, rawRegisters));
        break;
    }
    case DisplayFormat::Int32Unsigned: {
        const quint32 value = packBytes(rawRegisters.at(0), rawRegisters.at(1), settings.byteOrder);
        numericText = defaultScaling ? QString::number(qint64(value))
                                      : QString::number(numericValue(settings, rawRegisters));
        break;
    }
    default:
        break;
    }

    return withUnit(numericText, settings);
}

double numericValue(const FormatSettings &settings, const QList<quint16> &rawRegisters)
{
    switch (settings.format) {
    case DisplayFormat::Hex:
    case DisplayFormat::Binary:
        return double(rawRegisters.first());
    case DisplayFormat::SignedDecimal:
        return double(qint16(rawRegisters.first())) * settings.scale + settings.offset;
    case DisplayFormat::UnsignedDecimal:
        return double(rawRegisters.first()) * settings.scale + settings.offset;
    case DisplayFormat::Float32: {
        const float value = std::bit_cast<float>(packBytes(rawRegisters.at(0), rawRegisters.at(1), settings.byteOrder));
        return double(value) * settings.scale + settings.offset;
    }
    case DisplayFormat::Int32Signed: {
        const qint32 value = qint32(packBytes(rawRegisters.at(0), rawRegisters.at(1), settings.byteOrder));
        return double(value) * settings.scale + settings.offset;
    }
    case DisplayFormat::Int32Unsigned: {
        const quint32 value = packBytes(rawRegisters.at(0), rawRegisters.at(1), settings.byteOrder);
        return double(value) * settings.scale + settings.offset;
    }
    }
    return 0.0;
}

QList<quint16> parseValue(const FormatSettings &settings, const QString &text, bool *ok)
{
    if (ok)
        *ok = false;

    QString numericText = text;
    const QString suffix = QLatin1Char(' ') + settings.unit;
    if (!settings.unit.isEmpty() && numericText.endsWith(suffix))
        numericText.chop(suffix.size());

    if (settings.format == DisplayFormat::Hex) {
        QString hex = numericText;
        if (hex.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive))
            hex = hex.mid(2);
        bool parsed = false;
        const quint16 value = quint16(hex.toUInt(&parsed, 16));
        if (ok)
            *ok = parsed;
        return parsed ? QList<quint16>{value} : QList<quint16>{};
    }

    if (settings.format == DisplayFormat::Binary) {
        bool parsed = false;
        const quint16 value = quint16(numericText.toUInt(&parsed, 2));
        if (ok)
            *ok = parsed;
        return parsed ? QList<quint16>{value} : QList<quint16>{};
    }

    bool parsed = false;
    const double numeric = numericText.toDouble(&parsed);
    if (!parsed)
        return {};

    const bool defaultScaling = isDefaultScaling(settings);
    const double raw = defaultScaling ? numeric : (numeric - settings.offset) / settings.scale;

    if (ok)
        *ok = true;

    switch (settings.format) {
    case DisplayFormat::SignedDecimal:
        return {quint16(qint16(qRound64(raw)))};
    case DisplayFormat::UnsignedDecimal:
        return {quint16(qRound64(raw))};
    case DisplayFormat::Float32:
        return unpackToRegisters(std::bit_cast<quint32>(float(raw)), settings.byteOrder);
    case DisplayFormat::Int32Signed:
        return unpackToRegisters(quint32(qint32(qRound64(raw))), settings.byteOrder);
    case DisplayFormat::Int32Unsigned:
        return unpackToRegisters(quint32(qRound64(raw)), settings.byteOrder);
    default:
        if (ok)
            *ok = false;
        return {};
    }
}

} // namespace ModbusViewer::Core
