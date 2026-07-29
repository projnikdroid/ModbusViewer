#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>

#include "models/TagDatabaseModel.h"

namespace ModbusViewer::Core {
class ITagFileParser;
}

namespace ModbusViewer::AppLib {

// Drives a CSV/JSON register-map import into a caller-supplied TagDatabaseModel.
// Takes the model as a plain argument (like a QObject* passed between any two QML
// types) and calls addTags() on it directly in C++, rather than relaying the parsed
// QList<RegisterDefinition> back out through a signal into QML -- that would need
// custom metatype registration for a plain C++ struct to survive the QML boundary,
// exactly the kind of invisible-failure risk this project has hit before with QML/
// C++ mismatches (see CLAUDE.md's recurring gotchas).
class TagDatabaseController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit TagDatabaseController(QObject *parent = nullptr);

    Q_INVOKABLE void importCsv(const QString &filePath, TagDatabaseModel *model);
    Q_INVOKABLE void importJson(const QString &filePath, TagDatabaseModel *model);

signals:
    void importFinished(int count, const QStringList &errors);

private:
    void importUsing(Core::ITagFileParser &parser, const QString &filePath, TagDatabaseModel *model);
};

} // namespace ModbusViewer::AppLib
