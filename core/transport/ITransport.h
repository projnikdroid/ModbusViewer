#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

namespace ModbusViewer::Core {

// Abstracts TCP vs RTU/serial so the protocol and polling layers above don't care
// which transport is active. Both concrete transports (TcpTransport, SerialTransport)
// wrap an already-asynchronous Qt I/O class (QTcpSocket, QSerialPort) - there is no
// blocking call to hide, so this interface has no thread-related surface.
class ITransport : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    ~ITransport() override = default;

    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual qint64 write(const QByteArray &data) = 0;

    // TCP (via MBAP transaction ids) can have multiple requests outstanding at
    // once; RTU is strictly half-duplex and cannot. PollEngine uses this to decide
    // its in-flight request window size.
    virtual bool supportsPipelining() const = 0;

signals:
    void dataReceived(const QByteArray &data);
    void errorOccurred(const QString &message);
    void connectionStateChanged(bool connected);
};

} // namespace ModbusViewer::Core
