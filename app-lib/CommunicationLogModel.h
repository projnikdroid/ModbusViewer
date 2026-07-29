#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QString>

namespace ModbusViewer::AppLib {

// Live feed of every tx/rx frame plus transport/timeout errors, fed by
// ConnectionController::communicationLogged (see its header for the relay from
// ModbusTransactionManager). A fixed-capacity ring buffer -- continuous polling
// can produce a frame every few milliseconds, so an unbounded log would grow
// forever; oldest entries are evicted once MaxEntries is reached.
class CommunicationLogModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

public:
    enum Roles {
        TimestampRole = Qt::UserRole + 1,
        DirectionRole,
        SummaryRole,
    };

    enum class Direction { Tx, Rx, Error };
    Q_ENUM(Direction)

    static constexpr int MaxEntries = 500;

    explicit CommunicationLogModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void append(int direction, const QString &summary);
    Q_INVOKABLE void clear();

private:
    struct Entry
    {
        QString timestampText;
        Direction direction = Direction::Tx;
        QString summary;
    };

    QList<Entry> m_entries;
};

} // namespace ModbusViewer::AppLib
