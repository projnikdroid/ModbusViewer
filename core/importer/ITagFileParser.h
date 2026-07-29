#pragma once

#include <QIODevice>
#include <QList>
#include <QStringList>

#include "model/RegisterDefinition.h"

namespace ModbusViewer::Core {

struct TagParseResult
{
    QList<RegisterDefinition> tags;
    // One entry per bad row/item -- a malformed row is skipped, not fatal to the
    // rest of the import.
    QStringList errors;
};

class ITagFileParser
{
public:
    virtual ~ITagFileParser() = default;
    virtual TagParseResult parse(QIODevice &device) const = 0;
};

} // namespace ModbusViewer::Core
