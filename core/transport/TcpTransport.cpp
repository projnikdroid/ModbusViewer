#include "TcpTransport.h"

namespace ModbusViewer::Core {

TcpTransport::TcpTransport(QObject *parent)
    : ITransport(parent)
{
    connect(&m_socket, &QTcpSocket::readyRead, this, [this] { emit dataReceived(m_socket.readAll()); });
    connect(&m_socket, &QTcpSocket::connected, this, [this] { emit connectionStateChanged(true); });
    connect(&m_socket, &QTcpSocket::disconnected, this, [this] { emit connectionStateChanged(false); });
    connect(&m_socket, &QTcpSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) { emit errorOccurred(m_socket.errorString()); });
}

TcpTransport::~TcpTransport()
{
    // ~QTcpSocket disconnects the socket, which would fire our readyRead/disconnected
    // handlers and emit ITransport signals from an object that is already partway
    // through destruction - and reach observers whose own members may be gone.
    m_socket.disconnect(this);
}

void TcpTransport::setHost(const QString &host)
{
    m_host = host;
}

void TcpTransport::setPort(quint16 port)
{
    m_port = port;
}

bool TcpTransport::open()
{
    // A reconnect can find the socket still in a closing/errored state, where
    // connectToHost() would be ignored. Abort first so every attempt starts clean.
    m_socket.abort();

    // QTcpSocket::connectToHost() is itself asynchronous - this only starts the
    // attempt. The real outcome (success or failure) always arrives later via the
    // connectionStateChanged/errorOccurred signals wired up above, not this return
    // value.
    m_socket.connectToHost(m_host, m_port);
    return true;
}

void TcpTransport::close()
{
    m_socket.disconnectFromHost();
}

bool TcpTransport::isOpen() const
{
    return m_socket.state() == QAbstractSocket::ConnectedState;
}

qint64 TcpTransport::write(const QByteArray &data)
{
    return m_socket.write(data);
}

bool TcpTransport::supportsPipelining() const
{
    return true;
}

} // namespace ModbusViewer::Core
