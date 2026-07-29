#include "JsonTagParser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMap>

#include "importer/TagRowParser.h"

namespace ModbusViewer::Core {

TagParseResult JsonTagParser::parse(QIODevice &device) const
{
    TagParseResult result;
    const QByteArray data = device.readAll();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        result.errors.append(QStringLiteral("Invalid JSON: %1").arg(parseError.errorString()));
        return result;
    }

    if (!doc.isArray()) {
        result.errors.append(QStringLiteral("Expected a JSON array of tag objects at the root"));
        return result;
    }

    const QJsonArray items = doc.array();
    for (int i = 0; i < items.size(); ++i) {
        const QJsonValue item = items.at(i);
        if (!item.isObject()) {
            result.errors.append(QStringLiteral("Item %1: not an object").arg(i + 1));
            continue;
        }

        const QJsonObject object = item.toObject();
        QMap<QString, QString> fields;
        for (auto it = object.constBegin(); it != object.constEnd(); ++it)
            fields[it.key().toLower()] = it.value().toVariant().toString();

        const TagRowParseOutcome outcome = parseTagRow(fields);
        if (outcome.ok)
            result.tags.append(outcome.tag);
        else
            result.errors.append(QStringLiteral("Item %1: %2").arg(i + 1).arg(outcome.error));
    }

    return result;
}

} // namespace ModbusViewer::Core
