#include "FakeTransport.h"

namespace ModbusViewer::Test {

bool FakeTransport::open()
{
    if (!m_openShouldSucceed) {
        emit errorOccurred(QStringLiteral("simulated open failure"));
        return false;
    }
    m_open = true;
    emit connectionStateChanged(true);
    return true;
}

void FakeTransport::close()
{
    if (!m_open)
        return;
    m_open = false;
    emit connectionStateChanged(false);
}

bool FakeTransport::isOpen() const
{
    return m_open;
}

qint64 FakeTransport::write(const QByteArray &data)
{
    m_writtenChunks.append(data);
    return data.size();
}

bool FakeTransport::supportsPipelining() const
{
    return m_supportsPipelining;
}

void FakeTransport::setSupportsPipelining(bool supported)
{
    m_supportsPipelining = supported;
}

void FakeTransport::setOpenShouldSucceed(bool shouldSucceed)
{
    m_openShouldSucceed = shouldSucceed;
}

void FakeTransport::simulateDataReceived(const QByteArray &data)
{
    emit dataReceived(data);
}

void FakeTransport::simulateError(const QString &message)
{
    emit errorOccurred(message);
}

QList<QByteArray> FakeTransport::writtenChunks() const
{
    return m_writtenChunks;
}

} // namespace ModbusViewer::Test
