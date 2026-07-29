#include "SerialTransport.h"

#include <QThread>

#include "modbus/RtuTiming.h"

namespace ModbusViewer::Core {

SerialTransport::SerialTransport(QObject *parent)
    : ITransport(parent)
{
    connect(&m_port, &QSerialPort::readyRead, this, [this] { emit dataReceived(m_port.readAll()); });
    connect(&m_port, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
        if (error == QSerialPort::NoError)
            return;
        emit errorOccurred(m_port.errorString());
    });
}

SerialTransport::~SerialTransport()
{
    // See TcpTransport's destructor: closing the port during destruction must not
    // emit through a half-destroyed object.
    m_port.disconnect(this);
}

void SerialTransport::setSettings(const SerialPortSettings &settings)
{
    m_settings = settings;
}

bool SerialTransport::open()
{
    if (m_settings.portName.isEmpty()) {
        emit errorOccurred(QStringLiteral("No serial port selected."));
        return false;
    }

    m_port.setPortName(m_settings.portName);
    m_port.setBaudRate(m_settings.baudRate);
    m_port.setDataBits(m_settings.dataBits);
    m_port.setParity(m_settings.parity);
    m_port.setStopBits(m_settings.stopBits);
    m_port.setFlowControl(m_settings.flowControl);

    if (!m_port.open(QIODevice::ReadWrite)) {
        // Qt surfaces a bare OS message here ("The system cannot find the path
        // specified"), which says nothing about which port or why.
        emit errorOccurred(
            QStringLiteral("Could not open %1: %2").arg(m_settings.portName, m_port.errorString()));
        return false;
    }

    // Nothing has been sent yet, so the first write needs no silence beforehand.
    m_sinceLastFrame.invalidate();
    emit connectionStateChanged(true);
    return true;
}

void SerialTransport::close()
{
    if (!m_port.isOpen())
        return;
    m_port.close();
    emit connectionStateChanged(false);
}

bool SerialTransport::isOpen() const
{
    return m_port.isOpen();
}

qint64 SerialTransport::write(const QByteArray &data)
{
    waitForInterFrameSilence();

    const qint64 written = m_port.write(data);
    m_sinceLastFrame.restart();
    return written;
}

void SerialTransport::waitForInterFrameSilence()
{
    if (!m_sinceLastFrame.isValid())
        return; // first frame after opening the port

    const qint64 requiredUs = interFrameSilenceMicroseconds(m_settings.baudRate);
    const qint64 elapsedUs = m_sinceLastFrame.nsecsElapsed() / 1000;
    if (elapsedUs >= requiredUs)
        return;

    // The remaining gap is at most 1.75 ms (>19200 baud) or a few ms on slow links,
    // so a direct sleep is simpler than another timer hop and is short enough not to
    // be perceptible in the UI. PollEngine's own interval keeps sends far enough
    // apart that this rarely triggers at all.
    QThread::usleep(requiredUs - elapsedUs);
}

bool SerialTransport::supportsPipelining() const
{
    return false; // RTU is half-duplex: one transaction at a time
}

} // namespace ModbusViewer::Core
