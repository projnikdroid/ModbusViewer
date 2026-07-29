#include <QBuffer>
#include <QTest>

#include "importer/JsonTagParser.h"

using namespace ModbusViewer::Core;

namespace {
QBuffer *bufferFor(QByteArray &data)
{
    auto *buffer = new QBuffer(&data);
    buffer->open(QIODevice::ReadOnly);
    return buffer;
}
} // namespace

class JsonTagParserTest : public QObject
{
    Q_OBJECT

private slots:
    void validArrayImportsAllTags();
    void invalidJsonSyntaxProducesFileLevelError();
    void nonArrayRootProducesFileLevelError();
    void nonObjectItemIsSkippedWithError();
    void malformedItemIsSkippedNotAborting();
};

void JsonTagParserTest::validArrayImportsAllTags()
{
    QByteArray data = R"([
        { "label": "Tank Level", "registerType": "HoldingRegister", "address": 100,
          "format": "Float32", "byteOrder": "ABCD", "scale": 0.1, "offset": 0, "unit": "m" },
        { "label": "Pump Status", "registerType": "Coil", "address": 5 }
    ])";
    QScopedPointer<QBuffer> buffer(bufferFor(data));

    const JsonTagParser parser;
    const TagParseResult result = parser.parse(*buffer);

    QCOMPARE(result.errors.size(), 0);
    QCOMPARE(result.tags.size(), 2);
    QCOMPARE(result.tags.at(0).label, QStringLiteral("Tank Level"));
    QCOMPARE(result.tags.at(0).address, 100);
    QCOMPARE(result.tags.at(0).format.format, DisplayFormat::Float32);
    QCOMPARE(result.tags.at(1).registerType, RegisterType::Coil);
    QCOMPARE(result.tags.at(1).format.format, DisplayFormat::UnsignedDecimal);
}

void JsonTagParserTest::invalidJsonSyntaxProducesFileLevelError()
{
    QByteArray data = "{ not valid json ";
    QScopedPointer<QBuffer> buffer(bufferFor(data));

    const JsonTagParser parser;
    const TagParseResult result = parser.parse(*buffer);

    QCOMPARE(result.tags.size(), 0);
    QCOMPARE(result.errors.size(), 1);
}

void JsonTagParserTest::nonArrayRootProducesFileLevelError()
{
    QByteArray data = R"({ "label": "X" })";
    QScopedPointer<QBuffer> buffer(bufferFor(data));

    const JsonTagParser parser;
    const TagParseResult result = parser.parse(*buffer);

    QCOMPARE(result.tags.size(), 0);
    QCOMPARE(result.errors.size(), 1);
}

void JsonTagParserTest::nonObjectItemIsSkippedWithError()
{
    QByteArray data = R"([
        "not an object",
        { "label": "Good", "registerType": "Coil", "address": 1 }
    ])";
    QScopedPointer<QBuffer> buffer(bufferFor(data));

    const JsonTagParser parser;
    const TagParseResult result = parser.parse(*buffer);

    QCOMPARE(result.tags.size(), 1);
    QCOMPARE(result.tags.first().label, QStringLiteral("Good"));
    QCOMPARE(result.errors.size(), 1);
}

void JsonTagParserTest::malformedItemIsSkippedNotAborting()
{
    QByteArray data = R"([
        { "label": "Good One", "registerType": "Coil", "address": 1 },
        { "label": "Bad One", "registerType": "Nonsense", "address": 2 },
        { "label": "Good Two", "registerType": "Coil", "address": 3 }
    ])";
    QScopedPointer<QBuffer> buffer(bufferFor(data));

    const JsonTagParser parser;
    const TagParseResult result = parser.parse(*buffer);

    QCOMPARE(result.tags.size(), 2);
    QCOMPARE(result.tags.at(0).label, QStringLiteral("Good One"));
    QCOMPARE(result.tags.at(1).label, QStringLiteral("Good Two"));
    QCOMPARE(result.errors.size(), 1);
}

QTEST_APPLESS_MAIN(JsonTagParserTest)
#include "test_json_tag_parser.moc"
