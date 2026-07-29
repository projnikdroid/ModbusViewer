#include "RegisterFilterProxyModel.h"

namespace ModbusViewer::AppLib {

namespace {
const QList<QByteArray> kSearchableRoleNames = {"label", "description", "address", "unit"};
}

RegisterFilterProxyModel::RegisterFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

QString RegisterFilterProxyModel::filterText() const
{
    return m_filterText;
}

void RegisterFilterProxyModel::setFilterText(const QString &text)
{
    if (m_filterText == text)
        return;
    m_filterText = text;
    emit filterTextChanged();
    invalidateFilter();
}

int RegisterFilterProxyModel::mapRowToSource(int proxyRow) const
{
    return mapToSource(index(proxyRow, 0)).row();
}

bool RegisterFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (m_filterText.isEmpty())
        return true;
    if (!sourceModel())
        return true;

    const QHash<int, QByteArray> roles = sourceModel()->roleNames();
    const QModelIndex sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);

    for (const QByteArray &roleName : kSearchableRoleNames) {
        const int roleId = roles.key(roleName, -1);
        if (roleId < 0)
            continue;
        if (sourceModel()->data(sourceIndex, roleId).toString().contains(m_filterText, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

} // namespace ModbusViewer::AppLib
