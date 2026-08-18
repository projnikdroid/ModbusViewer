#include "SessionLogger.h"

#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QTime>
#include <QUrl>

namespace ModbusViewer::AppLib {

namespace {

// FileDialog.selectedFile from QML hands back a "file:///..." URL string; a
// plain native path (as used by this project's tests) has no such scheme and
// passes through unchanged. Duplicated from TagDatabaseController.cpp's
// identical helper -- at 2 call sites total this doesn't yet clear this
// project's bar for a shared utility.
QString toLocalPath(const QString &filePath)
{
    const QUrl url(filePath);
    if (url.isLocalFile())
        return url.toLocalFile();
    return filePath;
}

QString directionTag(int direction)
{
    switch (direction) {
    case 0:
        return QStringLiteral("[Tx]");
    case 1:
        return QStringLiteral("[Rx]");
    default:
        return QStringLiteral("[Err]");
    }
}

QString timestamp()
{
    return QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz"));
}

} // namespace

SessionLogger::SessionLogger(QObject *parent)
    : QObject(parent)
{
}

SessionLogger::~SessionLogger() = default;

bool SessionLogger::isLogging() const
{
    return m_stream != nullptr;
}

QString SessionLogger::logFilePath() const
{
    return m_logFilePath;
}

int SessionLogger::disconnectCount() const
{
    return m_disconnectCount;
}

int SessionLogger::reconnectCount() const
{
    return m_reconnectCount;
}

bool SessionLogger::startLogging(const QString &filePath)
{
    auto file = std::make_unique<QFile>(toLocalPath(filePath));
    if (!file->open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    m_file = std::move(file);
    m_stream = std::make_unique<QTextStream>(m_file.get());
    m_logFilePath = m_file->fileName();
    m_disconnectCount = 0;
    m_reconnectCount = 0;

    *m_stream << "=== ModbusViewer session log started " << QDateTime::currentDateTime().toString(Qt::ISODate)
               << " ===\n";
    m_stream->flush();

    emit isLoggingChanged();
    emit logFilePathChanged();
    emit disconnectCountChanged();
    emit reconnectCountChanged();
    return true;
}

void SessionLogger::stopLogging()
{
    if (!isLogging())
        return;

    *m_stream << "=== ModbusViewer session log ended -- " << m_disconnectCount << " disconnect(s), "
               << m_reconnectCount << " reconnect(s) ===\n";
    m_stream->flush();
    m_stream.reset();
    m_file.reset();

    emit isLoggingChanged();
}

void SessionLogger::logCommunication(int direction, const QString &summary)
{
    if (!isLogging())
        return;

    *m_stream << timestamp() << ' ' << directionTag(direction) << ' ' << summary << '\n';
    m_stream->flush();
}

void SessionLogger::recordDisconnect()
{
    if (!isLogging())
        return;

    ++m_disconnectCount;
    *m_stream << timestamp() << " [EVENT] Connection lost (disconnect #" << m_disconnectCount << ")\n";
    m_stream->flush();
    emit disconnectCountChanged();
}

void SessionLogger::recordReconnect()
{
    if (!isLogging())
        return;

    ++m_reconnectCount;
    *m_stream << timestamp() << " [EVENT] Reconnected (reconnect #" << m_reconnectCount << ")\n";
    m_stream->flush();
    emit reconnectCountChanged();
}

} // namespace ModbusViewer::AppLib
