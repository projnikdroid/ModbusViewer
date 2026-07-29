#include "SerialPortListModel.h"

#include <QSerialPortInfo>

namespace ModbusViewer::AppLib {

SerialPortListModel::SerialPortListModel(QObject *parent)
    : QAbstractListModel(parent)
{
    rescan();
}

int SerialPortListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_ports.size();
}

QVariant SerialPortListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_ports.size())
        return {};

    const PortInfo &port = m_ports.at(index.row());
    switch (role) {
    case PortNameRole:
        return port.portName;
    case DescriptionRole:
        return port.description;
    case DisplayLabelRole:
        return port.description.isEmpty() ? port.portName
                                          : QStringLiteral("%1 - %2").arg(port.portName, port.description);
    default:
        return {};
    }
}

QHash<int, QByteArray> SerialPortListModel::roleNames() const
{
    return {
        {PortNameRole, "portName"},
        {DescriptionRole, "description"},
        {DisplayLabelRole, "displayLabel"},
    };
}

void SerialPortListModel::rescan()
{
    beginResetModel();
    m_ports.clear();
    const auto availablePorts = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : availablePorts)
        m_ports.append({info.portName(), info.description()});
    endResetModel();
}

QString SerialPortListModel::portNameAt(int row) const
{
    if (row < 0 || row >= m_ports.size())
        return {};
    return m_ports.at(row).portName;
}

} // namespace ModbusViewer::AppLib
