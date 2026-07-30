#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QQmlEngine>
#include <QVariantMap>

#include "format/AddressConvention.h"
#include "model/RegisterDefinition.h"
#include "models/TagDatabaseModel.h"
#include "poll/PollTarget.h"

namespace ModbusViewer::AppLib {

// A curated, possibly-scattered list of registers polled as an alternative to
// Normal mode's single contiguous range (see docs/favorites-search-tags.md,
// "Favorites vs Normal -- one mode toggle, one PollEngine"). Each entry is a
// Core::RegisterDefinition -- the same "tag" struct M6a already designed with
// TagSource::AdHoc in mind for exactly this use -- plus its own live raw value.
class FavoritesModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(AddressConvention addressConvention READ addressConvention WRITE setAddressConvention NOTIFY
                   addressConventionChanged)

public:
    enum Roles {
        LabelRole = Qt::UserRole + 1,
        DescriptionRole,
        AddressRole,
        ValueRole,
        UnitRole,
        StaleRole,
        IsBitRole,
        BoolValueRole,
        WritableRole,
        HistoryRole,
    };

    enum class AddressConvention { Pdu, Modicon };
    Q_ENUM(AddressConvention)

    explicit FavoritesModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void addFromTag(TagDatabaseModel *tagModel, int row);
    Q_INVOKABLE void addAdHoc(int registerType, int address);
    Q_INVOKABLE void removeAt(int row);
    Q_INVOKABLE void clear();

    Q_INVOKABLE void setValueAt(int row, const QString &text);
    // Coil-only (DiscreteInput is read-only): no-op if the row's registerType isn't
    // Coil, else emits coilWriteRequested. The row's own bitValue is left for the
    // next applyBitUpdate to set, mirroring setValueAt's write-then-poll-confirms
    // pattern rather than optimistically updating local state.
    Q_INVOKABLE void setBitAt(int row, bool value);

    // Same contract as RegisterTableModel's -- FormatPicker.qml is written against
    // a duck-typed `registerModel` property calling exactly these two methods, so
    // it works against a FavoritesModel with no QML changes.
    Q_INVOKABLE QVariantMap formatSettingsAt(int row) const;
    Q_INVOKABLE void setFormatAt(int row, int format, int byteOrder, double scale, double offset,
                                 const QString &unit);

    AddressConvention addressConvention() const;
    void setAddressConvention(AddressConvention convention);

    // C++-only: called directly by ConnectionController, which holds the same
    // FavoritesModel* it received from QML via startPollingFavorites(). Never
    // crosses the QML boundary, so QList<PollTarget> needs no metatype
    // registration (same reasoning as TagDatabaseController's addTags()).
    QList<Core::PollTarget> buildPollTargets(quint8 unitId) const;
    void applyRegisterUpdate(int targetIndex, int startAddress, const QList<quint16> &values);
    void applyBitUpdate(int targetIndex, int startAddress, const QList<bool> &values);

    // Per-row staleness -- unlike RegisterTableModel's whole-range stale property,
    // each Favorites entry is its own independent PollTarget, so a poll failure
    // only affects the one row it was covering. C++-only, called directly by
    // ConnectionController through the same FavoritesModel* it already holds.
    void markStale(int row);

signals:
    void writeRequested(int address, int value);
    void coilWriteRequested(int address, bool value);
    void addressConventionChanged();

private:
    struct Entry
    {
        Core::RegisterDefinition definition;
        QList<quint16> rawValues;
        // Rolling window of numeric values for the Favorites card view's sparkline
        // (Core::numericValue()-derived, so already scale/offset-applied). Never
        // populated for bit-type entries -- a boolean has no meaningful trend.
        // Capped at kHistoryCapacity, oldest dropped first.
        QList<double> history;
        bool bitValue = false;
        bool stale = false;
    };

    Core::AddressConvention coreAddressConvention() const;

    QList<Entry> m_entries;
    AddressConvention m_addressConvention = AddressConvention::Pdu;
};

} // namespace ModbusViewer::AppLib
