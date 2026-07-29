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
// Scoped to HoldingRegister addressing for now, the only register type any view
// uses yet.
class RegisterTableModel : public QAbstractTableModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(AddressConvention addressConvention READ addressConvention WRITE setAddressConvention NOTIFY
                   addressConventionChanged)
    Q_PROPERTY(bool stale READ stale NOTIFY staleChanged)

public:
    enum Roles {
        AddressRole = Qt::UserRole + 1,
        ValueRole,
    };

    enum class AddressConvention { Pdu, Modicon };
    Q_ENUM(AddressConvention)

    explicit RegisterTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void setRegisters(int startAddress, const QList<int> &values);
    Q_INVOKABLE void setValueAt(int row, const QString &text);

    // Current (requested, not degraded-fallback) format settings for a row, keyed by
    // that row's starting address -- for FormatPicker.qml to prefill from.
    Q_INVOKABLE QVariantMap formatSettingsAt(int row) const;
    Q_INVOKABLE void setFormatAt(int row, int format, int byteOrder, double scale, double offset,
                                 const QString &unit);

    AddressConvention addressConvention() const;
    void setAddressConvention(AddressConvention convention);

    // Whole-range staleness: Normal mode is always exactly one PollTarget covering
    // the entire visible range, so a poll failure means the whole range is stale,
    // not any particular row (contrast FavoritesModel's per-row StaleRole, where
    // each entry is its own independent PollTarget).
    bool stale() const;
    Q_INVOKABLE void markStale();

signals:
    void writeRequested(int address, int value);
    void addressConventionChanged();
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
    QList<quint16> rawRegistersFor(const LogicalRow &row) const;

    int m_startAddress = 0;
    QList<quint16> m_rawValues;
    QMap<int, Core::FormatSettings> m_formats;
    AddressConvention m_addressConvention = AddressConvention::Pdu;

    bool m_stale = false;

    QList<LogicalRow> m_logicalRows;
    // Maps a raw register's index in m_rawValues to its logical row index, so a
    // per-poll-cycle value change can find which row to mark dirty without
    // rescanning every logical row.
    QList<int> m_rawIndexToRowIndex;
};

} // namespace ModbusViewer::AppLib
