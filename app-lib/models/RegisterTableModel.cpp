#include "RegisterTableModel.h"

#include "format/AddressConvention.h"
#include "model/RegisterType.h"

namespace ModbusViewer::AppLib {

RegisterTableModel::RegisterTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int RegisterTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_logicalRows.size();
}

int RegisterTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return 2;
}

QVariant RegisterTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_logicalRows.size())
        return {};

    const LogicalRow &row = m_logicalRows.at(index.row());
    switch (role) {
    case AddressRole: {
        const int displayStart =
            Core::displayAddress(Core::RegisterType::HoldingRegister, row.address, coreAddressConvention());
        if (row.span == 1)
            return QString::number(displayStart);
        const int displayEnd = Core::displayAddress(Core::RegisterType::HoldingRegister,
                                                     row.address + row.span - 1, coreAddressConvention());
        return QStringLiteral("%1-%2").arg(displayStart).arg(displayEnd);
    }
    case ValueRole:
        return Core::formatValue(row.settings, rawRegistersFor(row));
    default:
        return {};
    }
}

QHash<int, QByteArray> RegisterTableModel::roleNames() const
{
    return {
        {AddressRole, "address"},
        {ValueRole, "value"},
    };
}

void RegisterTableModel::setRegisters(int startAddress, const QList<int> &values)
{
    if (m_stale) {
        m_stale = false;
        emit staleChanged();
    }

    QList<quint16> newRaw;
    newRaw.reserve(values.size());
    for (int value : values)
        newRaw.append(quint16(value));

    const bool sameRawShape = m_startAddress == startAddress && m_rawValues.size() == newRaw.size();

    // A poll loop calls this every cycle. Resetting the model each time would tear
    // down and rebuild every delegate, so when only the raw values changed (not the
    // address range), update in place and report just the logical rows that
    // actually changed.
    if (sameRawShape) {
        int firstChangedRawIndex = -1;
        int lastChangedRawIndex = -1;
        for (int i = 0; i < newRaw.size(); ++i) {
            if (m_rawValues.at(i) == newRaw.at(i))
                continue;
            m_rawValues[i] = newRaw.at(i);
            if (firstChangedRawIndex < 0)
                firstChangedRawIndex = i;
            lastChangedRawIndex = i;
        }

        if (firstChangedRawIndex >= 0) {
            const int firstRow = m_rawIndexToRowIndex.at(firstChangedRawIndex);
            const int lastRow = m_rawIndexToRowIndex.at(lastChangedRawIndex);
            emit dataChanged(index(firstRow, 0), index(lastRow, columnCount() - 1), {ValueRole});
        }
        return;
    }

    beginResetModel();
    m_startAddress = startAddress;
    m_rawValues = newRaw;
    rebuildLogicalRows();
    endResetModel();
}

void RegisterTableModel::setValueAt(int row, const QString &text)
{
    if (row < 0 || row >= m_logicalRows.size())
        return;

    const LogicalRow &lr = m_logicalRows.at(row);
    bool ok = false;
    const QList<quint16> rawValues = Core::parseValue(lr.settings, text, &ok);
    if (!ok)
        return;

    for (int k = 0; k < rawValues.size(); ++k)
        emit writeRequested(lr.address + k, int(rawValues.at(k)));
}

QVariantMap RegisterTableModel::formatSettingsAt(int row) const
{
    QVariantMap result;
    if (row < 0 || row >= m_logicalRows.size())
        return result;

    // The requested settings, not the (possibly degraded-fallback) effective ones
    // used for rendering -- opening the picker on a temporarily-degraded row must
    // not silently discard what the user actually asked for.
    const Core::FormatSettings settings = m_formats.value(m_logicalRows.at(row).address);
    result["format"] = int(settings.format);
    result["byteOrder"] = int(settings.byteOrder);
    result["scale"] = settings.scale;
    result["offset"] = settings.offset;
    result["unit"] = settings.unit;
    return result;
}

void RegisterTableModel::setFormatAt(int row, int format, int byteOrder, double scale, double offset,
                                      const QString &unit)
{
    if (row < 0 || row >= m_logicalRows.size())
        return;

    const int address = m_logicalRows.at(row).address;

    Core::FormatSettings settings;
    settings.format = Core::DisplayFormat(format);
    settings.byteOrder = Core::ByteOrder(byteOrder);
    settings.scale = scale;
    settings.offset = offset;
    settings.unit = unit;
    m_formats[address] = settings;

    beginResetModel();
    rebuildLogicalRows();
    endResetModel();
}

bool RegisterTableModel::stale() const
{
    return m_stale;
}

void RegisterTableModel::markStale()
{
    if (m_stale)
        return;
    m_stale = true;
    emit staleChanged();
}

RegisterTableModel::AddressConvention RegisterTableModel::addressConvention() const
{
    return m_addressConvention;
}

void RegisterTableModel::setAddressConvention(AddressConvention convention)
{
    if (m_addressConvention == convention)
        return;
    m_addressConvention = convention;
    emit addressConventionChanged();
    if (!m_logicalRows.isEmpty())
        emit dataChanged(index(0, 0), index(m_logicalRows.size() - 1, columnCount() - 1), {AddressRole});
}

void RegisterTableModel::rebuildLogicalRows()
{
    m_logicalRows.clear();
    m_rawIndexToRowIndex.clear();
    m_rawIndexToRowIndex.resize(m_rawValues.size());

    int i = 0;
    while (i < m_rawValues.size()) {
        const int address = m_startAddress + i;
        const Core::FormatSettings requested = m_formats.value(address);
        const int neededSpan = Core::registerSpanFor(requested.format);
        const bool canFit = i + neededSpan <= m_rawValues.size();

        LogicalRow row;
        row.address = address;
        row.span = canFit ? neededSpan : 1;
        row.settings = canFit ? requested : Core::FormatSettings{};

        const int rowIndex = m_logicalRows.size();
        for (int k = 0; k < row.span; ++k)
            m_rawIndexToRowIndex[i + k] = rowIndex;

        m_logicalRows.append(row);
        i += row.span;
    }
}

Core::AddressConvention RegisterTableModel::coreAddressConvention() const
{
    return m_addressConvention == AddressConvention::Modicon ? Core::AddressConvention::Modicon
                                                              : Core::AddressConvention::Pdu;
}

QList<quint16> RegisterTableModel::rawRegistersFor(const LogicalRow &row) const
{
    const int offset = row.address - m_startAddress;
    return m_rawValues.mid(offset, row.span);
}

} // namespace ModbusViewer::AppLib
