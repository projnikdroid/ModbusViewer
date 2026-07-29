#include "TagDatabaseController.h"

#include <QFile>
#include <QUrl>

#include "importer/CsvTagParser.h"
#include "importer/ITagFileParser.h"
#include "importer/JsonTagParser.h"
#include "models/TagDatabaseModel.h"

namespace ModbusViewer::AppLib {

namespace {

// FileDialog.selectedFile/currentFile from QML hands back a "file:///..." URL
// string; a plain native path (as used by this project's tests) has no such
// scheme and passes through unchanged.
QString toLocalPath(const QString &filePath)
{
    const QUrl url(filePath);
    if (url.isLocalFile())
        return url.toLocalFile();
    return filePath;
}

} // namespace

TagDatabaseController::TagDatabaseController(QObject *parent)
    : QObject(parent)
{
}

void TagDatabaseController::importUsing(Core::ITagFileParser &parser, const QString &filePath,
                                         TagDatabaseModel *model)
{
    QFile file(toLocalPath(filePath));
    if (!file.open(QIODevice::ReadOnly)) {
        emit importFinished(0, {QStringLiteral("Could not open file: %1").arg(filePath)});
        return;
    }

    const Core::TagParseResult result = parser.parse(file);
    if (model)
        model->addTags(result.tags);

    emit importFinished(result.tags.size(), result.errors);
}

void TagDatabaseController::importCsv(const QString &filePath, TagDatabaseModel *model)
{
    Core::CsvTagParser parser;
    importUsing(parser, filePath, model);
}

void TagDatabaseController::importJson(const QString &filePath, TagDatabaseModel *model)
{
    Core::JsonTagParser parser;
    importUsing(parser, filePath, model);
}

} // namespace ModbusViewer::AppLib
