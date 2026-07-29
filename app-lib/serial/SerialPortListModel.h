#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QString>

namespace ModbusViewer::AppLib {

// Available serial ports, scanned once on construction and again on demand via
// rescan() (Decision 19: no background polling for device changes in v1 - the
// connection screen offers an explicit Rescan action for hot-plugged adapters).
class SerialPortListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

public:
    enum Roles {
        PortNameRole = Qt::UserRole + 1,
        DescriptionRole,
        DisplayLabelRole,
    };

    explicit SerialPortListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void rescan();
    Q_INVOKABLE QString portNameAt(int row) const;

private:
    struct PortInfo
    {
        QString portName;
        QString description;
    };
    QList<PortInfo> m_ports;
};

} // namespace ModbusViewer::AppLib
