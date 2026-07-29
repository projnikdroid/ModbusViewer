#include "TagRowParser.h"

namespace ModbusViewer::Core {

namespace {

bool matches(const QString &text, const char *name)
{
    return text.compare(QLatin1String(name), Qt::CaseInsensitive) == 0;
}

bool parseRegisterTypeName(const QString &text, RegisterType *out)
{
    if (matches(text, "Coil")) { *out = RegisterType::Coil; return true; }
    if (matches(text, "DiscreteInput")) { *out = RegisterType::DiscreteInput; return true; }
    if (matches(text, "HoldingRegister")) { *out = RegisterType::HoldingRegister; return true; }
    if (matches(text, "InputRegister")) { *out = RegisterType::InputRegister; return true; }
    return false;
}

bool parseDisplayFormatName(const QString &text, DisplayFormat *out)
{
    if (matches(text, "SignedDecimal")) { *out = DisplayFormat::SignedDecimal; return true; }
    if (matches(text, "UnsignedDecimal")) { *out = DisplayFormat::UnsignedDecimal; return true; }
    if (matches(text, "Hex")) { *out = DisplayFormat::Hex; return true; }
    if (matches(text, "Binary")) { *out = DisplayFormat::Binary; return true; }
    if (matches(text, "Float32")) { *out = DisplayFormat::Float32; return true; }
    if (matches(text, "Int32Signed")) { *out = DisplayFormat::Int32Signed; return true; }
    if (matches(text, "Int32Unsigned")) { *out = DisplayFormat::Int32Unsigned; return true; }
    return false;
}

bool parseByteOrderName(const QString &text, ByteOrder *out)
{
    if (matches(text, "ABCD")) { *out = ByteOrder::ABCD; return true; }
    if (matches(text, "BADC")) { *out = ByteOrder::BADC; return true; }
    if (matches(text, "CDAB")) { *out = ByteOrder::CDAB; return true; }
    if (matches(text, "DCBA")) { *out = ByteOrder::DCBA; return true; }
    return false;
}

} // namespace

TagRowParseOutcome parseTagRow(const QMap<QString, QString> &fields)
{
    TagRowParseOutcome outcome;

    const QString label = fields.value(QStringLiteral("label")).trimmed();
    if (label.isEmpty()) {
        outcome.error = QStringLiteral("missing required field 'label'");
        return outcome;
    }

    const QString registerTypeText = fields.value(QStringLiteral("registertype")).trimmed();
    RegisterType registerType;
    if (!parseRegisterTypeName(registerTypeText, &registerType)) {
        outcome.error = QStringLiteral("unrecognized registerType '%1'").arg(registerTypeText);
        return outcome;
    }

    const QString addressText = fields.value(QStringLiteral("address")).trimmed();
    bool addressOk = false;
    const int address = addressText.toInt(&addressOk);
    if (!addressOk || address < 0) {
        outcome.error = QStringLiteral("invalid address '%1'").arg(addressText);
        return outcome;
    }

    FormatSettings format;

    const QString formatText = fields.value(QStringLiteral("format")).trimmed();
    if (!formatText.isEmpty() && !parseDisplayFormatName(formatText, &format.format)) {
        outcome.error = QStringLiteral("unrecognized format '%1'").arg(formatText);
        return outcome;
    }

    const QString byteOrderText = fields.value(QStringLiteral("byteorder")).trimmed();
    if (!byteOrderText.isEmpty() && !parseByteOrderName(byteOrderText, &format.byteOrder)) {
        outcome.error = QStringLiteral("unrecognized byteOrder '%1'").arg(byteOrderText);
        return outcome;
    }

    const QString scaleText = fields.value(QStringLiteral("scale")).trimmed();
    if (!scaleText.isEmpty()) {
        bool scaleOk = false;
        format.scale = scaleText.toDouble(&scaleOk);
        if (!scaleOk) {
            outcome.error = QStringLiteral("invalid scale '%1'").arg(scaleText);
            return outcome;
        }
    }

    const QString offsetText = fields.value(QStringLiteral("offset")).trimmed();
    if (!offsetText.isEmpty()) {
        bool offsetOk = false;
        format.offset = offsetText.toDouble(&offsetOk);
        if (!offsetOk) {
            outcome.error = QStringLiteral("invalid offset '%1'").arg(offsetText);
            return outcome;
        }
    }

    format.unit = fields.value(QStringLiteral("unit")).trimmed();

    outcome.tag.label = label;
    outcome.tag.description = fields.value(QStringLiteral("description")).trimmed();
    outcome.tag.registerType = registerType;
    outcome.tag.address = address;
    outcome.tag.format = format;
    outcome.tag.source = TagSource::Imported;
    outcome.ok = true;
    return outcome;
}

} // namespace ModbusViewer::Core
