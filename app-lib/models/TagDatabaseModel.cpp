#include "TagDatabaseModel.h"

namespace ModbusViewer::AppLib {

TagDatabaseModel::TagDatabaseModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int TagDatabaseModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_tags.size();
}

QVariant TagDatabaseModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tags.size())
        return {};

    const Core::RegisterDefinition &tag = m_tags.at(index.row());
    switch (role) {
    case LabelRole:
        return tag.label;
    case DescriptionRole:
        return tag.description;
    case AddressRole:
        return tag.address;
    case RegisterTypeRole:
        return int(tag.registerType);
    case FormatRole:
        return int(tag.format.format);
    case ByteOrderRole:
        return int(tag.format.byteOrder);
    case ScaleRole:
        return tag.format.scale;
    case OffsetRole:
        return tag.format.offset;
    case UnitRole:
        return tag.format.unit;
    default:
        return {};
    }
}

QHash<int, QByteArray> TagDatabaseModel::roleNames() const
{
    return {
        {LabelRole, "label"},
        {DescriptionRole, "description"},
        {AddressRole, "address"},
        {RegisterTypeRole, "registerType"},
        {FormatRole, "format"},
        {ByteOrderRole, "byteOrder"},
        {ScaleRole, "scale"},
        {OffsetRole, "offset"},
        {UnitRole, "unit"},
    };
}

void TagDatabaseModel::addTags(const QList<Core::RegisterDefinition> &tags)
{
    if (tags.isEmpty())
        return;

    beginInsertRows(QModelIndex(), m_tags.size(), m_tags.size() + tags.size() - 1);
    m_tags.append(tags);
    endInsertRows();
}

const Core::RegisterDefinition &TagDatabaseModel::tagAt(int row) const
{
    static const Core::RegisterDefinition empty;
    if (row < 0 || row >= m_tags.size())
        return empty;
    return m_tags.at(row);
}

void TagDatabaseModel::clear()
{
    if (m_tags.isEmpty())
        return;

    beginResetModel();
    m_tags.clear();
    endResetModel();
}

} // namespace ModbusViewer::AppLib
