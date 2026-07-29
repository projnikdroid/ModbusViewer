#pragma once

#include <QElapsedTimer>
#include <QSerialPort>

#include "ITransport.h"

namespace ModbusViewer::Core {

struct SerialPortSettings
{
    QString portName;
    int baudRate = 115200;
    QSerialPort::DataBits dataBits = QSerialPort::Data8;
    QSerialPort::Parity parity = QSerialPort::EvenParity;
    QSerialPort::StopBits stopBits = QSerialPort::OneStop;
    QSerialPort::FlowControl flowControl = QSerialPort::NoFlowControl;
};

// Wraps QSerialPort. Unlike TCP, RTU is strictly half-duplex - one transaction at a
// time - and the spec requires a minimum silence between frames (see RtuTiming).
// write() enforces that silence itself so callers cannot accidentally skip it.
class SerialTransport : public ITransport
{
    Q_OBJECT

public:
    explicit SerialTransport(QObject *parent = nullptr);
    ~SerialTransport() override;

    void setSettings(const SerialPortSettings &settings);

    bool open() override;
    void close() override;
    bool isOpen() const override;
    qint64 write(const QByteArray &data) override;
    bool supportsPipelining() const override;

private:
    void waitForInterFrameSilence();

    QSerialPort m_port;
    SerialPortSettings m_settings;
    QElapsedTimer m_sinceLastFrame;
};

} // namespace ModbusViewer::Core
