#include "FavoritesModel.h"

#include "format/AddressConvention.h"

namespace ModbusViewer::AppLib {

using namespace ModbusViewer::Core;

FavoritesModel::FavoritesModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int FavoritesModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_entries.size();
}

QVariant FavoritesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case LabelRole:
        return entry.definition.label;
    case DescriptionRole:
        return entry.definition.description;
    case AddressRole:
        return QString::number(
            Core::displayAddress(entry.definition.registerType, entry.definition.address, coreAddressConvention()));
    case ValueRole:
        return Core::formatValue(entry.definition.format, entry.rawValues);
    case UnitRole:
        return entry.definition.format.unit;
    case StaleRole:
        return entry.stale;
    default:
        return {};
    }
}

QHash<int, QByteArray> FavoritesModel::roleNames() const
{
    return {
        {LabelRole, "label"},
        {DescriptionRole, "description"},
        {AddressRole, "address"},
        {ValueRole, "value"},
        {UnitRole, "unit"},
        {StaleRole, "stale"},
    };
}

void FavoritesModel::addFromTag(TagDatabaseModel *tagModel, int row)
{
    if (!tagModel)
        return;

    Entry entry;
    entry.definition = tagModel->tagAt(row);
    entry.rawValues = QList<quint16>(entry.definition.registerSpan(), 0);

    beginInsertRows(QModelIndex(), m_entries.size(), m_entries.size());
    m_entries.append(entry);
    endInsertRows();
}

void FavoritesModel::addAdHoc(int registerType, int address)
{
    Entry entry;
    entry.definition.label = QString::number(address);
    entry.definition.registerType = RegisterType(registerType);
    entry.definition.address = address;
    entry.definition.source = TagSource::AdHoc;
    entry.rawValues = QList<quint16>(entry.definition.registerSpan(), 0);

    beginInsertRows(QModelIndex(), m_entries.size(), m_entries.size());
    m_entries.append(entry);
    endInsertRows();
}

void FavoritesModel::removeAt(int row)
{
    if (row < 0 || row >= m_entries.size())
        return;

    beginRemoveRows(QModelIndex(), row, row);
    m_entries.removeAt(row);
    endRemoveRows();
}

void FavoritesModel::clear()
{
    if (m_entries.isEmpty())
        return;

    beginResetModel();
    m_entries.clear();
    endResetModel();
}

void FavoritesModel::setValueAt(int row, const QString &text)
{
    if (row < 0 || row >= m_entries.size())
        return;

    const Entry &entry = m_entries.at(row);
    bool ok = false;
    const QList<quint16> rawValues = Core::parseValue(entry.definition.format, text, &ok);
    if (!ok)
        return;

    for (int k = 0; k < rawValues.size(); ++k)
        emit writeRequested(entry.definition.address + k, int(rawValues.at(k)));
}

QVariantMap FavoritesModel::formatSettingsAt(int row) const
{
    QVariantMap result;
    if (row < 0 || row >= m_entries.size())
        return result;

    const FormatSettings &settings = m_entries.at(row).definition.format;
    result["format"] = int(settings.format);
    result["byteOrder"] = int(settings.byteOrder);
    result["scale"] = settings.scale;
    result["offset"] = settings.offset;
    result["unit"] = settings.unit;
    return result;
}

void FavoritesModel::setFormatAt(int row, int format, int byteOrder, double scale, double offset,
                                 const QString &unit)
{
    if (row < 0 || row >= m_entries.size())
        return;

    Entry &entry = m_entries[row];
    entry.definition.format.format = DisplayFormat(format);
    entry.definition.format.byteOrder = ByteOrder(byteOrder);
    entry.definition.format.scale = scale;
    entry.definition.format.offset = offset;
    entry.definition.format.unit = unit;
    entry.rawValues = QList<quint16>(entry.definition.registerSpan(), 0);

    emit dataChanged(index(row, 0), index(row, 0));
}

FavoritesModel::AddressConvention FavoritesModel::addressConvention() const
{
    return m_addressConvention;
}

void FavoritesModel::setAddressConvention(AddressConvention convention)
{
    if (m_addressConvention == convention)
        return;
    m_addressConvention = convention;
    emit addressConventionChanged();
    if (!m_entries.isEmpty())
        emit dataChanged(index(0, 0), index(m_entries.size() - 1, 0), {AddressRole});
}

QList<PollTarget> FavoritesModel::buildPollTargets(quint8 unitId) const
{
    QList<PollTarget> targets;
    targets.reserve(m_entries.size());
    for (const Entry &entry : m_entries) {
        PollTarget target;
        target.unitId = unitId;
        target.registerType = entry.definition.registerType;
        target.startAddress = quint16(entry.definition.address);
        target.quantity = quint16(entry.definition.registerSpan());
        targets.append(target);
    }
    return targets;
}

void FavoritesModel::applyRegisterUpdate(int targetIndex, int startAddress, const QList<quint16> &values)
{
    Q_UNUSED(startAddress);
    if (targetIndex < 0 || targetIndex >= m_entries.size())
        return;

    m_entries[targetIndex].rawValues = values;
    m_entries[targetIndex].stale = false;
    emit dataChanged(index(targetIndex, 0), index(targetIndex, 0), {ValueRole, StaleRole});
}

void FavoritesModel::markStale(int row)
{
    if (row < 0 || row >= m_entries.size())
        return;
    if (m_entries.at(row).stale)
        return;

    m_entries[row].stale = true;
    emit dataChanged(index(row, 0), index(row, 0), {StaleRole});
}

Core::AddressConvention FavoritesModel::coreAddressConvention() const
{
    return m_addressConvention == AddressConvention::Modicon ? Core::AddressConvention::Modicon
                                                              : Core::AddressConvention::Pdu;
}

} // namespace ModbusViewer::AppLib
