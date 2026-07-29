#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QQmlEngine>

#include "model/RegisterDefinition.h"

namespace ModbusViewer::AppLib {

// Holds every tag imported so far (CSV and/or JSON imports accumulate rather than
// replace each other -- a "database" you keep adding register maps to). Read by the
// Favorites picker and Normal-view tag lookup (M6c, not built yet).
class TagDatabaseModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

public:
    enum Roles {
        LabelRole = Qt::UserRole + 1,
        DescriptionRole,
        AddressRole,
        RegisterTypeRole,
        FormatRole,
        ByteOrderRole,
        ScaleRole,
        OffsetRole,
        UnitRole,
    };

    explicit TagDatabaseModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void addTags(const QList<ModbusViewer::Core::RegisterDefinition> &tags);
    Q_INVOKABLE void clear();

    // C++-only accessor for callers that already hold a Core::RegisterDefinition
    // (e.g. FavoritesModel::addFromTag) -- relaying the struct back out through a
    // signal into QML would need metatype registration it doesn't have (see M6a).
    // Out-of-range returns a default-constructed definition, matching this class's
    // existing defensive style.
    const ModbusViewer::Core::RegisterDefinition &tagAt(int row) const;

private:
    QList<Core::RegisterDefinition> m_tags;
};

} // namespace ModbusViewer::AppLib
