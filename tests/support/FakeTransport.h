#pragma once

#include <QByteArray>
#include <QList>

#include "transport/ITransport.h"

namespace ModbusViewer::Test {

// Test double for ITransport: no real I/O. A test script controls exactly what the
// transport "receives" and inspects exactly what was "sent", including simulating
// errors and configuring pipelining support. Used by the transport-contract test
// here, and later by PollEngine tests that need controllable latency/ordering.
class FakeTransport : public ModbusViewer::Core::ITransport
{
    Q_OBJECT

public:
    using ITransport::ITransport;

    bool open() override;
    void close() override;
    bool isOpen() const override;
    qint64 write(const QByteArray &data) override;
    bool supportsPipelining() const override;

    void setSupportsPipelining(bool supported);
    void setOpenShouldSucceed(bool shouldSucceed);

    void simulateDataReceived(const QByteArray &data);
    void simulateError(const QString &message);

    QList<QByteArray> writtenChunks() const;

private:
    bool m_open = false;
    bool m_supportsPipelining = false;
    bool m_openShouldSucceed = true;
    QList<QByteArray> m_writtenChunks;
};

} // namespace ModbusViewer::Test
