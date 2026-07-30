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
        const int displayStart = Core::displayAddress(coreRegisterType(), row.address, coreAddressConvention());
        if (row.span == 1)
            return QString::number(displayStart);
        const int displayEnd =
            Core::displayAddress(coreRegisterType(), row.address + row.span - 1, coreAddressConvention());
        return QStringLiteral("%1-%2").arg(displayStart).arg(displayEnd);
    }
    case ValueRole:
        if (Core::isBitRegisterType(coreRegisterType()))
            return m_bitValues.value(row.address - m_startAddress) ? QStringLiteral("ON") : QStringLiteral("OFF");
        return Core::formatValue(row.settings, rawRegistersFor(row));
    case IsBitRole:
        return Core::isBitRegisterType(coreRegisterType());
    case BoolValueRole:
        return Core::isBitRegisterType(coreRegisterType()) && m_bitValues.value(row.address - m_startAddress);
    case WritableRole:
        return Core::isWritableRegisterType(coreRegisterType());
    default:
        return {};
    }
}

QHash<int, QByteArray> RegisterTableModel::roleNames() const
{
    return {
        {AddressRole, "address"},
        {ValueRole, "value"},
        {IsBitRole, "isBit"},
        {BoolValueRole, "boolValue"},
        {WritableRole, "writable"},
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

void RegisterTableModel::setBits(int startAddress, const QList<bool> &values)
{
    if (m_stale) {
        m_stale = false;
        emit staleChanged();
    }

    const bool sameShape = m_startAddress == startAddress && m_bitValues.size() == values.size();

    // Same diff-in-place vs. full-reset split as setRegisters(), for the same
    // reason: preserve flash-on-update, which needs onValueChanged to fire on an
    // existing delegate rather than one torn down and rebuilt.
    if (sameShape) {
        int firstChangedRow = -1;
        int lastChangedRow = -1;
        for (int i = 0; i < values.size(); ++i) {
            if (m_bitValues.at(i) == values.at(i))
                continue;
            m_bitValues[i] = values.at(i);
            if (firstChangedRow < 0)
                firstChangedRow = i;
            lastChangedRow = i;
        }

        if (firstChangedRow >= 0)
            emit dataChanged(index(firstChangedRow, 0), index(lastChangedRow, columnCount() - 1),
                              {ValueRole, BoolValueRole});
        return;
    }

    beginResetModel();
    m_startAddress = startAddress;
    m_bitValues = values;
    rebuildLogicalRows();
    endResetModel();
}

void RegisterTableModel::setValueAt(int row, const QString &text)
{
    if (row < 0 || row >= m_logicalRows.size())
        return;
    if (Core::isBitRegisterType(coreRegisterType()))
        return; // bit rows go through setBitAt, not text parsing

    const LogicalRow &lr = m_logicalRows.at(row);
    bool ok = false;
    const QList<quint16> rawValues = Core::parseValue(lr.settings, text, &ok);
    if (!ok)
        return;

    for (int k = 0; k < rawValues.size(); ++k)
        emit writeRequested(lr.address + k, int(rawValues.at(k)));
}

void RegisterTableModel::setBitAt(int row, bool value)
{
    if (row < 0 || row >= m_logicalRows.size())
        return;
    if (m_registerType != RegisterType::Coil)
        return; // DiscreteInput is read-only; word types go through setValueAt

    emit coilWriteRequested(m_logicalRows.at(row).address, value);
}

int RegisterTableModel::maxReadCountFor() const
{
    return Core::maxReadCountFor(coreRegisterType());
}

QVariantMap RegisterTableModel::formatSettingsAt(int row) const
{
    QVariantMap result;
    if (row < 0 || row >= m_logicalRows.size())
        return result;
    if (Core::isBitRegisterType(coreRegisterType()))
        return result; // scale/offset/unit/byteOrder/format have no meaning for a bit

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
    if (Core::isBitRegisterType(coreRegisterType()))
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

RegisterTableModel::RegisterType RegisterTableModel::registerType() const
{
    return m_registerType;
}

void RegisterTableModel::setRegisterType(RegisterType type)
{
    if (m_registerType == type)
        return;

    // Switching address space invalidates whatever was being displayed -- a stale
    // format keyed by address from the old space (e.g. a Float32 picked for a
    // HoldingRegister) has no meaning once the space changes.
    beginResetModel();
    m_registerType = type;
    m_rawValues.clear();
    m_bitValues.clear();
    m_formats.clear();
    rebuildLogicalRows();
    endResetModel();

    emit registerTypeChanged();
}

void RegisterTableModel::rebuildLogicalRows()
{
    m_logicalRows.clear();
    m_rawIndexToRowIndex.clear();

    if (Core::isBitRegisterType(coreRegisterType())) {
        m_rawIndexToRowIndex.resize(m_bitValues.size());
        for (int i = 0; i < m_bitValues.size(); ++i) {
            LogicalRow row;
            row.address = m_startAddress + i;
            row.span = 1;
            m_rawIndexToRowIndex[i] = i;
            m_logicalRows.append(row);
        }
        return;
    }

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

Core::RegisterType RegisterTableModel::coreRegisterType() const
{
    switch (m_registerType) {
    case RegisterType::Coil:
        return Core::RegisterType::Coil;
    case RegisterType::DiscreteInput:
        return Core::RegisterType::DiscreteInput;
    case RegisterType::HoldingRegister:
        return Core::RegisterType::HoldingRegister;
    case RegisterType::InputRegister:
        return Core::RegisterType::InputRegister;
    }
    return Core::RegisterType::HoldingRegister;
}

QList<quint16> RegisterTableModel::rawRegistersFor(const LogicalRow &row) const
{
    const int offset = row.address - m_startAddress;
    return m_rawValues.mid(offset, row.span);
}

} // namespace ModbusViewer::AppLib
