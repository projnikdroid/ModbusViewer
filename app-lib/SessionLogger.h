#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <memory>

class QFile;
class QTextStream;

namespace ModbusViewer::AppLib {

// Persists the live Communication Log to a user-chosen file, plus connect/
// disconnect/reconnect event lines with running counts. Off by default --
// QML instantiates this once (like CommunicationLogModel) and calls
// startLogging()/stopLogging() from an explicit user action, never
// automatically. Uses QFile + QTextStream rather than QSaveFile: this is a
// continuously-appended live log, and QSaveFile only commits its content on
// close, which would lose everything if the app crashed mid-session.
class SessionLogger : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool isLogging READ isLogging NOTIFY isLoggingChanged)
    Q_PROPERTY(QString logFilePath READ logFilePath NOTIFY logFilePathChanged)
    Q_PROPERTY(int disconnectCount READ disconnectCount NOTIFY disconnectCountChanged)
    Q_PROPERTY(int reconnectCount READ reconnectCount NOTIFY reconnectCountChanged)

public:
    explicit SessionLogger(QObject *parent = nullptr);
    ~SessionLogger() override;

    bool isLogging() const;
    QString logFilePath() const;
    int disconnectCount() const;
    int reconnectCount() const;

    // filePath may be a "file:///..." URL (as handed back by QML's FileDialog) or a
    // plain native path. Resets both counts to 0 for the new session. Returns false
    // (and leaves isLogging false) if the file can't be opened.
    Q_INVOKABLE bool startLogging(const QString &filePath);
    Q_INVOKABLE void stopLogging();

    // direction matches CommunicationLogModel::Direction's underlying values
    // (0=Tx, 1=Rx, 2=Error). No-op while not logging.
    Q_INVOKABLE void logCommunication(int direction, const QString &summary);

    // No-op while not logging -- QML calls these unconditionally whenever
    // ConnectionController emits connectionLost/connectionRestored.
    Q_INVOKABLE void recordDisconnect();
    Q_INVOKABLE void recordReconnect();

signals:
    void isLoggingChanged();
    void logFilePathChanged();
    void disconnectCountChanged();
    void reconnectCountChanged();

private:
    std::unique_ptr<QFile> m_file;
    std::unique_ptr<QTextStream> m_stream;
    QString m_logFilePath;
    int m_disconnectCount = 0;
    int m_reconnectCount = 0;
};

} // namespace ModbusViewer::AppLib
