#include <QTest>

#include "format/ValueFormatter.h"

using namespace ModbusViewer::Core;

class ValueFormatterTest : public QObject
{
    Q_OBJECT

private slots:
    void registerSpanMatchesFormat();

    void unsignedDecimalShowsRawValue();
    void signedDecimalWrapsAtHalfRange();
    void hexIsFourUppercaseDigitsWithPrefix();
    void binaryIsSixteenBits();

    void float32DecodesAcrossAllByteOrders();
    void int32DecodesAcrossAllByteOrders();
    void int32SignedVsUnsignedInterpretSameBitsDifferently();

    void scaleAndOffsetApplyToDecimalFormats();
    void unitIsAppendedToDecimalFormats();
    void hexAndBinaryIgnoreScaleOffsetAndUnit();

    void parseValueRoundTripsFormatValue_data();
    void parseValueRoundTripsFormatValue();

    void numericValueMatchesFormatValueForDecimalFormats();
    void numericValueForHexAndBinaryIsRawRegisterValue();
};

void ValueFormatterTest::registerSpanMatchesFormat()
{
    QCOMPARE(registerSpanFor(DisplayFormat::SignedDecimal), 1);
    QCOMPARE(registerSpanFor(DisplayFormat::UnsignedDecimal), 1);
    QCOMPARE(registerSpanFor(DisplayFormat::Hex), 1);
    QCOMPARE(registerSpanFor(DisplayFormat::Binary), 1);
    QCOMPARE(registerSpanFor(DisplayFormat::Float32), 2);
    QCOMPARE(registerSpanFor(DisplayFormat::Int32Signed), 2);
    QCOMPARE(registerSpanFor(DisplayFormat::Int32Unsigned), 2);
}

void ValueFormatterTest::unsignedDecimalShowsRawValue()
{
    FormatSettings settings;
    settings.format = DisplayFormat::UnsignedDecimal;
    QCOMPARE(formatValue(settings, {quint16(0xFFFF)}), QStringLiteral("65535"));
    QCOMPARE(formatValue(settings, {quint16(0)}), QStringLiteral("0"));
}

void ValueFormatterTest::signedDecimalWrapsAtHalfRange()
{
    FormatSettings settings;
    settings.format = DisplayFormat::SignedDecimal;
    QCOMPARE(formatValue(settings, {quint16(0xFFFF)}), QStringLiteral("-1"));
    QCOMPARE(formatValue(settings, {quint16(0x8000)}), QStringLiteral("-32768"));
    QCOMPARE(formatValue(settings, {quint16(0x0001)}), QStringLiteral("1"));
}

void ValueFormatterTest::hexIsFourUppercaseDigitsWithPrefix()
{
    FormatSettings settings;
    settings.format = DisplayFormat::Hex;
    QCOMPARE(formatValue(settings, {quint16(0x00FF)}), QStringLiteral("0x00FF"));
    QCOMPARE(formatValue(settings, {quint16(0xABCD)}), QStringLiteral("0xABCD"));
}

void ValueFormatterTest::binaryIsSixteenBits()
{
    FormatSettings settings;
    settings.format = DisplayFormat::Binary;
    QCOMPARE(formatValue(settings, {quint16(0x00FF)}), QStringLiteral("0000000011111111"));
    QCOMPARE(formatValue(settings, {quint16(0)}), QStringLiteral("0000000000000000"));
}

// 1.5f == 0x3FC00000. ABCD is the standard big-endian byte order: register0 carries
// the high-order 16 bits (0x3FC0), register1 the low-order 16 bits (0x0000). The
// other three orders are byte/word-swapped permutations of the same four bytes.
void ValueFormatterTest::float32DecodesAcrossAllByteOrders()
{
    FormatSettings settings;
    settings.format = DisplayFormat::Float32;

    settings.byteOrder = ByteOrder::ABCD;
    QCOMPARE(formatValue(settings, {quint16(0x3FC0), quint16(0x0000)}), QStringLiteral("1.5"));

    settings.byteOrder = ByteOrder::BADC;
    QCOMPARE(formatValue(settings, {quint16(0xC03F), quint16(0x0000)}), QStringLiteral("1.5"));

    settings.byteOrder = ByteOrder::CDAB;
    QCOMPARE(formatValue(settings, {quint16(0x0000), quint16(0x3FC0)}), QStringLiteral("1.5"));

    settings.byteOrder = ByteOrder::DCBA;
    QCOMPARE(formatValue(settings, {quint16(0x0000), quint16(0xC03F)}), QStringLiteral("1.5"));
}

// 0x12345678 has four distinct bytes so each byte order produces a distinct raw
// register pair, unlike an all-equal-byte value which couldn't tell them apart.
void ValueFormatterTest::int32DecodesAcrossAllByteOrders()
{
    FormatSettings settings;
    settings.format = DisplayFormat::Int32Unsigned;

    settings.byteOrder = ByteOrder::ABCD;
    QCOMPARE(formatValue(settings, {quint16(0x1234), quint16(0x5678)}), QStringLiteral("305419896"));

    settings.byteOrder = ByteOrder::BADC;
    QCOMPARE(formatValue(settings, {quint16(0x3412), quint16(0x7856)}), QStringLiteral("305419896"));

    settings.byteOrder = ByteOrder::CDAB;
    QCOMPARE(formatValue(settings, {quint16(0x5678), quint16(0x1234)}), QStringLiteral("305419896"));

    settings.byteOrder = ByteOrder::DCBA;
    QCOMPARE(formatValue(settings, {quint16(0x7856), quint16(0x3412)}), QStringLiteral("305419896"));
}

void ValueFormatterTest::int32SignedVsUnsignedInterpretSameBitsDifferently()
{
    // 0x80000001 as ABCD: reg0 = 0x8000, reg1 = 0x0001.
    FormatSettings unsignedSettings;
    unsignedSettings.format = DisplayFormat::Int32Unsigned;
    unsignedSettings.byteOrder = ByteOrder::ABCD;
    QCOMPARE(formatValue(unsignedSettings, {quint16(0x8000), quint16(0x0001)}),
             QStringLiteral("2147483649"));

    FormatSettings signedSettings;
    signedSettings.format = DisplayFormat::Int32Signed;
    signedSettings.byteOrder = ByteOrder::ABCD;
    QCOMPARE(formatValue(signedSettings, {quint16(0x8000), quint16(0x0001)}),
             QStringLiteral("-2147483647"));
}

void ValueFormatterTest::scaleAndOffsetApplyToDecimalFormats()
{
    FormatSettings settings;
    settings.format = DisplayFormat::UnsignedDecimal;
    settings.scale = 0.1;
    settings.offset = 5.0;
    // raw 100 * 0.1 + 5 = 15
    QCOMPARE(formatValue(settings, {quint16(100)}), QStringLiteral("15"));
}

void ValueFormatterTest::unitIsAppendedToDecimalFormats()
{
    FormatSettings settings;
    settings.format = DisplayFormat::UnsignedDecimal;
    settings.scale = 0.1;
    settings.offset = 5.0;
    settings.unit = QStringLiteral("°C");
    QCOMPARE(formatValue(settings, {quint16(100)}), QStringLiteral("15 °C"));
}

void ValueFormatterTest::hexAndBinaryIgnoreScaleOffsetAndUnit()
{
    FormatSettings settings;
    settings.format = DisplayFormat::Hex;
    settings.scale = 2.0;
    settings.offset = 100.0;
    settings.unit = QStringLiteral("V");
    QCOMPARE(formatValue(settings, {quint16(0x00FF)}), QStringLiteral("0x00FF"));

    settings.format = DisplayFormat::Binary;
    QCOMPARE(formatValue(settings, {quint16(0x00FF)}), QStringLiteral("0000000011111111"));
}

void ValueFormatterTest::parseValueRoundTripsFormatValue_data()
{
    QTest::addColumn<int>("format");
    QTest::addColumn<int>("byteOrder");
    QTest::addColumn<QList<quint16>>("rawRegisters");

    QTest::newRow("SignedDecimal negative")
        << int(DisplayFormat::SignedDecimal) << int(ByteOrder::ABCD)
        << QList<quint16>{quint16(0xFFFF)};
    QTest::newRow("UnsignedDecimal")
        << int(DisplayFormat::UnsignedDecimal) << int(ByteOrder::ABCD)
        << QList<quint16>{quint16(4321)};
    QTest::newRow("Hex")
        << int(DisplayFormat::Hex) << int(ByteOrder::ABCD) << QList<quint16>{quint16(0xBEEF)};
    QTest::newRow("Binary")
        << int(DisplayFormat::Binary) << int(ByteOrder::ABCD) << QList<quint16>{quint16(0x00FF)};
    QTest::newRow("Float32 ABCD") << int(DisplayFormat::Float32) << int(ByteOrder::ABCD)
                                  << QList<quint16>{quint16(0x3FC0), quint16(0x0000)};
    QTest::newRow("Float32 DCBA") << int(DisplayFormat::Float32) << int(ByteOrder::DCBA)
                                  << QList<quint16>{quint16(0x0000), quint16(0xC03F)};
    QTest::newRow("Int32Signed CDAB")
        << int(DisplayFormat::Int32Signed) << int(ByteOrder::CDAB)
        << QList<quint16>{quint16(0x5678), quint16(0x1234)};
    QTest::newRow("Int32Unsigned BADC")
        << int(DisplayFormat::Int32Unsigned) << int(ByteOrder::BADC)
        << QList<quint16>{quint16(0x3412), quint16(0x7856)};
}

void ValueFormatterTest::parseValueRoundTripsFormatValue()
{
    QFETCH(int, format);
    QFETCH(int, byteOrder);
    QFETCH(QList<quint16>, rawRegisters);

    FormatSettings settings;
    settings.format = DisplayFormat(format);
    settings.byteOrder = ByteOrder(byteOrder);

    const QString text = formatValue(settings, rawRegisters);

    bool ok = false;
    const QList<quint16> parsed = parseValue(settings, text, &ok);
    QVERIFY(ok);
    QCOMPARE(parsed, rawRegisters);
}

// numericValue() exists for callers (FavoritesModel's sparkline history) that need
// a plottable double rather than formatValue()'s display string -- these values
// mirror the raw numbers already asserted (via their stringified form) above.
void ValueFormatterTest::numericValueMatchesFormatValueForDecimalFormats()
{
    FormatSettings unsignedSettings;
    unsignedSettings.format = DisplayFormat::UnsignedDecimal;
    QCOMPARE(numericValue(unsignedSettings, {quint16(65535)}), 65535.0);

    FormatSettings signedSettings;
    signedSettings.format = DisplayFormat::SignedDecimal;
    QCOMPARE(numericValue(signedSettings, {quint16(0xFFFF)}), -1.0);

    FormatSettings scaledSettings;
    scaledSettings.format = DisplayFormat::UnsignedDecimal;
    scaledSettings.scale = 0.1;
    scaledSettings.offset = 5.0;
    QCOMPARE(numericValue(scaledSettings, {quint16(100)}), 15.0);

    FormatSettings floatSettings;
    floatSettings.format = DisplayFormat::Float32;
    floatSettings.byteOrder = ByteOrder::ABCD;
    QCOMPARE(numericValue(floatSettings, {quint16(0x3FC0), quint16(0x0000)}), 1.5);

    FormatSettings int32Settings;
    int32Settings.format = DisplayFormat::Int32Unsigned;
    int32Settings.byteOrder = ByteOrder::ABCD;
    QCOMPARE(numericValue(int32Settings, {quint16(0x1234), quint16(0x5678)}), 305419896.0);
}

// Hex/Binary have no scale/offset semantics -- numericValue() returns the plain
// raw register value, ignoring any scale/offset set on the FormatSettings, matching
// formatValue()'s existing "Hex/Binary always show the raw bit pattern" rule.
void ValueFormatterTest::numericValueForHexAndBinaryIsRawRegisterValue()
{
    FormatSettings settings;
    settings.format = DisplayFormat::Hex;
    settings.scale = 2.0;
    settings.offset = 100.0;
    QCOMPARE(numericValue(settings, {quint16(0x00FF)}), 255.0);

    settings.format = DisplayFormat::Binary;
    QCOMPARE(numericValue(settings, {quint16(0x00FF)}), 255.0);
}

QTEST_APPLESS_MAIN(ValueFormatterTest)
#include "test_value_formatter.moc"
