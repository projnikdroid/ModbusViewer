#include "CsvTagParser.h"

#include <QMap>
#include <QTextStream>

#include "importer/TagRowParser.h"

namespace ModbusViewer::Core {

namespace {

QStringList splitCsvLine(const QString &line)
{
    return line.split(QLatin1Char(','));
}

} // namespace

TagParseResult CsvTagParser::parse(QIODevice &device) const
{
    TagParseResult result;
    QTextStream in(&device);

    QString headerLine;
    if (!in.readLineInto(&headerLine)) {
        result.errors.append(QStringLiteral("Empty file: no header row found"));
        return result;
    }

    const QStringList headerColumns = splitCsvLine(headerLine);
    QMap<QString, int> columnIndex;
    for (int i = 0; i < headerColumns.size(); ++i)
        columnIndex[headerColumns.at(i).trimmed().toLower()] = i;

    static const QStringList requiredColumns{QStringLiteral("label"), QStringLiteral("registertype"),
                                              QStringLiteral("address")};
    QStringList missingColumns;
    for (const QString &required : requiredColumns) {
        if (!columnIndex.contains(required))
            missingColumns.append(required);
    }
    if (!missingColumns.isEmpty()) {
        result.errors.append(
            QStringLiteral("Missing required column(s): %1").arg(missingColumns.join(QStringLiteral(", "))));
        return result;
    }

    int lineNumber = 1; // the header itself is line 1
    QString line;
    while (in.readLineInto(&line)) {
        ++lineNumber;
        if (line.trimmed().isEmpty())
            continue;

        const QStringList values = splitCsvLine(line);
        QMap<QString, QString> fields;
        for (auto it = columnIndex.constBegin(); it != columnIndex.constEnd(); ++it) {
            const int index = it.value();
            fields[it.key()] = index < values.size() ? values.at(index) : QString();
        }

        const TagRowParseOutcome outcome = parseTagRow(fields);
        if (outcome.ok)
            result.tags.append(outcome.tag);
        else
            result.errors.append(QStringLiteral("Row %1: %2").arg(lineNumber).arg(outcome.error));
    }

    return result;
}

} // namespace ModbusViewer::Core
