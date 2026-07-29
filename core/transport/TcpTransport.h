#pragma once

#include <QTcpSocket>

#include "ITransport.h"

namespace ModbusViewer::Core {

// Wraps QTcpSocket, which is already asynchronous under the Qt event loop - open()
// starts a non-blocking connectToHost(), and the socket's own readyRead/errorOccurred
// signals drive dataReceived/errorOccurred here. Supports pipelining: Modbus TCP's
// MBAP transaction ids allow multiple outstanding requests on one socket.
class TcpTransport : public ITransport
{
    Q_OBJECT

public:
    explicit TcpTransport(QObject *parent = nullptr);
    ~TcpTransport() override;

    void setHost(const QString &host);
    void setPort(quint16 port);

    bool open() override;
    void close() override;
    bool isOpen() const override;
    qint64 write(const QByteArray &data) override;
    bool supportsPipelining() const override;

private:
    QTcpSocket m_socket;
    QString m_host;
    quint16 m_port = 502;
};

} // namespace ModbusViewer::Core
