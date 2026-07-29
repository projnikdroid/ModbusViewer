#pragma once

#include <QQmlEngine>
#include <QSortFilterProxyModel>
#include <QString>

namespace ModbusViewer::AppLib {

// A single free-text filter reused across every register/tag list in the app
// (Normal view, Favorites, the tag picker) despite their differing role sets --
// searchable roles are looked up *by name* against whatever the current
// sourceModel() actually exposes, so a model with only address/value (
// RegisterTableModel) is matched on address alone, while one with label/
// description/unit too (FavoritesModel, TagDatabaseModel) is matched on all of
// them. Never inspects the value role, which is what lets live polling keep
// rendering through an active filter -- a value-only update can't change which
// rows match. See docs/favorites-search-tags.md, "Search".
class RegisterFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)

public:
    explicit RegisterFilterProxyModel(QObject *parent = nullptr);

    QString filterText() const;
    void setFilterText(const QString &text);

    // QAbstractProxyModel::mapToSource() isn't Q_INVOKABLE, so QML delegates
    // (whose row is the proxy's row, not the source model's) need this to find
    // which source row a write/format-picker call should actually target.
    Q_INVOKABLE int mapRowToSource(int proxyRow) const;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

signals:
    void filterTextChanged();

private:
    QString m_filterText;
};

} // namespace ModbusViewer::AppLib
