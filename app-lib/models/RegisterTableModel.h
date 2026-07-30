#pragma once

#include <QAbstractTableModel>
#include <QList>
#include <QMap>
#include <QQmlEngine>
#include <QVariantMap>

#include "format/AddressConvention.h"
#include "format/ValueFormatter.h"

namespace ModbusViewer::AppLib {

// One row per register by default; a row whose format is Float32/Int32Signed/
// Int32Unsigned consumes its address and the next one, merging into a single row
// (M6 — see docs/protocol.md "Addressing convention" and the plan's M6 verify step).
// Bit types (Coil/DiscreteInput) are always one row per bit -- span 1, no format
// applies.
class RegisterTableModel : public QAbstractTableModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(AddressConvention addressConvention READ addressConvention WRITE setAddressConvention NOTIFY
                   addressConventionChanged)
    Q_PROPERTY(RegisterType registerType READ registerType WRITE setRegisterType NOTIFY registerTypeChanged)
    Q_PROPERTY(bool stale READ stale NOTIFY staleChanged)

public:
    enum Roles {
        AddressRole = Qt::UserRole + 1,
        ValueRole,
        IsBitRole,
        BoolValueRole,
        WritableRole,
    };

    enum class AddressConvention { Pdu, Modicon };
    Q_ENUM(AddressConvention)

    // Matches Core::RegisterType's ordinals 1:1 (same idiom as AddressConvention
    // above) so QML can bind this by raw int, e.g. the same values already used by
    // MainScreen.qml's Favorites ad-hoc-add combo.
    enum class RegisterType { Coil, DiscreteInput, HoldingRegister, InputRegister };
    Q_ENUM(RegisterType)

    explicit RegisterTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void setRegisters(int startAddress, const QList<int> &values);
    Q_INVOKABLE void setBits(int startAddress, const QList<bool> &values);
    Q_INVOKABLE void setValueAt(int row, const QString &text);
    // Coil-only (DiscreteInput is read-only): no-op if the current registerType
    // isn't Coil, else emits coilWriteRequested. Mirrors setValueAt's
    // write-then-poll-confirms pattern -- no optimistic local update.
    Q_INVOKABLE void setBitAt(int row, bool value);

    // Thin wrapper for QML's quantity SpinBox: 125 for word types, 2000 for bit
    // types, per the Modbus spec's per-request ceilings.
    Q_INVOKABLE int maxReadCountFor() const;

    // Current (requested, not degraded-fallback) format settings for a row, keyed by
    // that row's starting address -- for FormatPicker.qml to prefill from.
    Q_INVOKABLE QVariantMap formatSettingsAt(int row) const;
    Q_INVOKABLE void setFormatAt(int row, int format, int byteOrder, double scale, double offset,
                                 const QString &unit);

    AddressConvention addressConvention() const;
    void setAddressConvention(AddressConvention convention);

    RegisterType registerType() const;
    void setRegisterType(RegisterType type);

    // Whole-range staleness: Normal mode is always exactly one PollTarget covering
    // the entire visible range, so a poll failure means the whole range is stale,
    // not any particular row (contrast FavoritesModel's per-row StaleRole, where
    // each entry is its own independent PollTarget).
    bool stale() const;
    Q_INVOKABLE void markStale();

signals:
    void writeRequested(int address, int value);
    void coilWriteRequested(int address, bool value);
    void addressConventionChanged();
    void registerTypeChanged();
    void staleChanged();

private:
    struct LogicalRow
    {
        int address = 0;
        int span = 1;
        Core::FormatSettings settings;
    };

    void rebuildLogicalRows();
    Core::AddressConvention coreAddressConvention() const;
    Core::RegisterType coreRegisterType() const;
    QList<quint16> rawRegistersFor(const LogicalRow &row) const;

    int m_startAddress = 0;
    QList<quint16> m_rawValues;
    QList<bool> m_bitValues;
    QMap<int, Core::FormatSettings> m_formats;
    AddressConvention m_addressConvention = AddressConvention::Pdu;
    RegisterType m_registerType = RegisterType::HoldingRegister;

    bool m_stale = false;

    QList<LogicalRow> m_logicalRows;
    // Maps a raw register's index in m_rawValues to its logical row index, so a
    // per-poll-cycle value change can find which row to mark dirty without
    // rescanning every logical row.
    QList<int> m_rawIndexToRowIndex;
};

} // namespace ModbusViewer::AppLib
