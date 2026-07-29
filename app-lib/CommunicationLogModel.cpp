#include "CommunicationLogModel.h"

#include <QTime>

namespace ModbusViewer::AppLib {

CommunicationLogModel::CommunicationLogModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int CommunicationLogModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_entries.size();
}

QVariant CommunicationLogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case TimestampRole:
        return entry.timestampText;
    case DirectionRole:
        return int(entry.direction);
    case SummaryRole:
        return entry.summary;
    default:
        return {};
    }
}

QHash<int, QByteArray> CommunicationLogModel::roleNames() const
{
    return {
        {TimestampRole, "timestamp"},
        {DirectionRole, "direction"},
        {SummaryRole, "summary"},
    };
}

void CommunicationLogModel::append(int direction, const QString &summary)
{
    if (m_entries.size() >= MaxEntries) {
        beginRemoveRows(QModelIndex(), 0, 0);
        m_entries.removeFirst();
        endRemoveRows();
    }

    Entry entry;
    entry.timestampText = QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    entry.direction = Direction(direction);
    entry.summary = summary;

    beginInsertRows(QModelIndex(), m_entries.size(), m_entries.size());
    m_entries.append(entry);
    endInsertRows();
}

void CommunicationLogModel::clear()
{
    if (m_entries.isEmpty())
        return;

    beginResetModel();
    m_entries.clear();
    endResetModel();
}

} // namespace ModbusViewer::AppLib
