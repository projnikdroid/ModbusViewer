#include <QTest>

#include "importer/TagRowParser.h"

using namespace ModbusViewer::Core;

class TagRowParserTest : public QObject
{
    Q_OBJECT

private slots:
    void parsesAFullyPopulatedRow();
    void appliesDefaultsForOmittedOptionalFields();
    void isCaseInsensitiveForEnumFields();
    void rejectsMissingLabel();
    void rejectsUnrecognizedRegisterType();
    void rejectsInvalidAddress();
    void rejectsNegativeAddress();
    void rejectsUnrecognizedFormat();
    void rejectsUnrecognizedByteOrder();
    void rejectsInvalidScale();
    void rejectsInvalidOffset();
};

void TagRowParserTest::parsesAFullyPopulatedRow()
{
    const QMap<QString, QString> fields{
        {"label", "Tank Level"},
        {"description", "North tank"},
        {"registertype", "HoldingRegister"},
        {"address", "100"},
        {"format", "Float32"},
        {"byteorder", "DCBA"},
        {"scale", "0.1"},
        {"offset", "5"},
        {"unit", "m"},
    };

    const TagRowParseOutcome outcome = parseTagRow(fields);
    QVERIFY(outcome.ok);
    QCOMPARE(outcome.tag.label, QStringLiteral("Tank Level"));
    QCOMPARE(outcome.tag.description, QStringLiteral("North tank"));
    QCOMPARE(outcome.tag.registerType, RegisterType::HoldingRegister);
    QCOMPARE(outcome.tag.address, 100);
    QCOMPARE(outcome.tag.format.format, DisplayFormat::Float32);
    QCOMPARE(outcome.tag.format.byteOrder, ByteOrder::DCBA);
    QCOMPARE(outcome.tag.format.scale, 0.1);
    QCOMPARE(outcome.tag.format.offset, 5.0);
    QCOMPARE(outcome.tag.format.unit, QStringLiteral("m"));
    QCOMPARE(outcome.tag.source, TagSource::Imported);
}

void TagRowParserTest::appliesDefaultsForOmittedOptionalFields()
{
    const QMap<QString, QString> fields{
        {"label", "Status"},
        {"registertype", "Coil"},
        {"address", "0"},
    };

    const TagRowParseOutcome outcome = parseTagRow(fields);
    QVERIFY(outcome.ok);
    QCOMPARE(outcome.tag.format.format, DisplayFormat::UnsignedDecimal);
    QCOMPARE(outcome.tag.format.byteOrder, ByteOrder::ABCD);
    QCOMPARE(outcome.tag.format.scale, 1.0);
    QCOMPARE(outcome.tag.format.offset, 0.0);
    QVERIFY(outcome.tag.format.unit.isEmpty());
    QVERIFY(outcome.tag.description.isEmpty());
}

void TagRowParserTest::isCaseInsensitiveForEnumFields()
{
    const QMap<QString, QString> fields{
        {"label", "X"},
        {"registertype", "inputregister"},
        {"address", "1"},
        {"format", "float32"},
        {"byteorder", "badc"},
    };

    const TagRowParseOutcome outcome = parseTagRow(fields);
    QVERIFY(outcome.ok);
    QCOMPARE(outcome.tag.registerType, RegisterType::InputRegister);
    QCOMPARE(outcome.tag.format.format, DisplayFormat::Float32);
    QCOMPARE(outcome.tag.format.byteOrder, ByteOrder::BADC);
}

void TagRowParserTest::rejectsMissingLabel()
{
    const QMap<QString, QString> fields{{"registertype", "Coil"}, {"address", "0"}};
    const TagRowParseOutcome outcome = parseTagRow(fields);
    QVERIFY(!outcome.ok);
    QVERIFY(!outcome.error.isEmpty());
}

void TagRowParserTest::rejectsUnrecognizedRegisterType()
{
    const QMap<QString, QString> fields{{"label", "X"}, {"registertype", "Nonsense"}, {"address", "0"}};
    QVERIFY(!parseTagRow(fields).ok);
}

void TagRowParserTest::rejectsInvalidAddress()
{
    const QMap<QString, QString> fields{{"label", "X"}, {"registertype", "Coil"}, {"address", "abc"}};
    QVERIFY(!parseTagRow(fields).ok);
}

void TagRowParserTest::rejectsNegativeAddress()
{
    const QMap<QString, QString> fields{{"label", "X"}, {"registertype", "Coil"}, {"address", "-1"}};
    QVERIFY(!parseTagRow(fields).ok);
}

void TagRowParserTest::rejectsUnrecognizedFormat()
{
    const QMap<QString, QString> fields{
        {"label", "X"}, {"registertype", "Coil"}, {"address", "0"}, {"format", "Nonsense"}};
    QVERIFY(!parseTagRow(fields).ok);
}

void TagRowParserTest::rejectsUnrecognizedByteOrder()
{
    const QMap<QString, QString> fields{
        {"label", "X"}, {"registertype", "Coil"}, {"address", "0"}, {"byteorder", "Nonsense"}};
    QVERIFY(!parseTagRow(fields).ok);
}

void TagRowParserTest::rejectsInvalidScale()
{
    const QMap<QString, QString> fields{
        {"label", "X"}, {"registertype", "Coil"}, {"address", "0"}, {"scale", "abc"}};
    QVERIFY(!parseTagRow(fields).ok);
}

void TagRowParserTest::rejectsInvalidOffset()
{
    const QMap<QString, QString> fields{
        {"label", "X"}, {"registertype", "Coil"}, {"address", "0"}, {"offset", "abc"}};
    QVERIFY(!parseTagRow(fields).ok);
}

QTEST_APPLESS_MAIN(TagRowParserTest)
#include "test_tag_row_parser.moc"
