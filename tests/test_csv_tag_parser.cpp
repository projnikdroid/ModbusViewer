#include <QBuffer>
#include <QTest>

#include "importer/CsvTagParser.h"

using namespace ModbusViewer::Core;

namespace {
QBuffer *bufferFor(QByteArray &data)
{
    auto *buffer = new QBuffer(&data);
    buffer->open(QIODevice::ReadOnly);
    return buffer;
}
} // namespace

class CsvTagParserTest : public QObject
{
    Q_OBJECT

private slots:
    void validMultiRowCsvImportsAllTags();
    void missingRequiredColumnProducesFileLevelError();
    void malformedRowIsSkippedNotAborting();
    void blankLinesAreSkipped();
    void columnOrderAndHeaderCaseAreFlexible();
    void emptyFileProducesFileLevelError();
};

void CsvTagParserTest::validMultiRowCsvImportsAllTags()
{
    QByteArray data =
        "label,description,registerType,address,format,byteOrder,scale,offset,unit\n"
        "Tank Level,North tank,HoldingRegister,100,Float32,ABCD,0.1,0,m\n"
        "Pump Status,,Coil,5,,,,,\n";
    QScopedPointer<QBuffer> buffer(bufferFor(data));

    const CsvTagParser parser;
    const TagParseResult result = parser.parse(*buffer);

    QCOMPARE(result.errors.size(), 0);
    QCOMPARE(result.tags.size(), 2);
    QCOMPARE(result.tags.at(0).label, QStringLiteral("Tank Level"));
    QCOMPARE(result.tags.at(0).address, 100);
    QCOMPARE(result.tags.at(0).format.format, DisplayFormat::Float32);
    QCOMPARE(result.tags.at(1).label, QStringLiteral("Pump Status"));
    QCOMPARE(result.tags.at(1).registerType, RegisterType::Coil);
    QCOMPARE(result.tags.at(1).format.format, DisplayFormat::UnsignedDecimal);
}

void CsvTagParserTest::missingRequiredColumnProducesFileLevelError()
{
    QByteArray data = "label,registerType\nX,Coil\n";
    QScopedPointer<QBuffer> buffer(bufferFor(data));

    const CsvTagParser parser;
    const TagParseResult result = parser.parse(*buffer);

    QCOMPARE(result.tags.size(), 0);
    QCOMPARE(result.errors.size(), 1);
}

void CsvTagParserTest::malformedRowIsSkippedNotAborting()
{
    QByteArray data =
        "label,registerType,address\n"
        "Good One,Coil,1\n"
        "Bad One,Coil,not-a-number\n"
        "Good Two,Coil,2\n";
    QScopedPointer<QBuffer> buffer(bufferFor(data));

    const CsvTagParser parser;
    const TagParseResult result = parser.parse(*buffer);

    QCOMPARE(result.tags.size(), 2);
    QCOMPARE(result.tags.at(0).label, QStringLiteral("Good One"));
    QCOMPARE(result.tags.at(1).label, QStringLiteral("Good Two"));
    QCOMPARE(result.errors.size(), 1);
    QVERIFY(result.errors.first().contains(QStringLiteral("3"))); // row 3 (1 header + 2)
}

void CsvTagParserTest::blankLinesAreSkipped()
{
    QByteArray data =
        "label,registerType,address\n"
        "First,Coil,1\n"
        "\n"
        "Second,Coil,2\n";
    QScopedPointer<QBuffer> buffer(bufferFor(data));

    const CsvTagParser parser;
    const TagParseResult result = parser.parse(*buffer);

    QCOMPARE(result.tags.size(), 2);
    QCOMPARE(result.errors.size(), 0);
}

void CsvTagParserTest::columnOrderAndHeaderCaseAreFlexible()
{
    QByteArray data =
        "ADDRESS,Label,registertype\n"
        "42,Widget,InputRegister\n";
    QScopedPointer<QBuffer> buffer(bufferFor(data));

    const CsvTagParser parser;
    const TagParseResult result = parser.parse(*buffer);

    QCOMPARE(result.errors.size(), 0);
    QCOMPARE(result.tags.size(), 1);
    QCOMPARE(result.tags.first().address, 42);
    QCOMPARE(result.tags.first().label, QStringLiteral("Widget"));
    QCOMPARE(result.tags.first().registerType, RegisterType::InputRegister);
}

void CsvTagParserTest::emptyFileProducesFileLevelError()
{
    QByteArray data = "";
    QScopedPointer<QBuffer> buffer(bufferFor(data));

    const CsvTagParser parser;
    const TagParseResult result = parser.parse(*buffer);

    QCOMPARE(result.tags.size(), 0);
    QCOMPARE(result.errors.size(), 1);
}

QTEST_APPLESS_MAIN(CsvTagParserTest)
#include "test_csv_tag_parser.moc"
