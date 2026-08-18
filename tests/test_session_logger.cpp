#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "SessionLogger.h"

using namespace ModbusViewer::AppLib;

namespace {
QString logPathIn(const QTemporaryDir &dir)
{
    return dir.filePath(QStringLiteral("session.log"));
}

QString readWholeFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return QString::fromUtf8(file.readAll());
}
} // namespace

class SessionLoggerTest : public QObject
{
    Q_OBJECT

private slots:
    void startLoggingOpensTheFileAndSetsIsLoggingTrue();
    void startLoggingOnAnUnwritablePathReturnsFalseAndIsLoggingStaysFalse();
    void logCommunicationWritesALineOnlyWhileLogging();
    void recordDisconnectAndRecordReconnectIncrementCountsAndWriteEventLines();
    void stopLoggingWritesTheFooterSummaryAndClearsIsLogging();
    void startingASecondLoggingSessionResetsBothCountsToZero();
};

void SessionLoggerTest::startLoggingOpensTheFileAndSetsIsLoggingTrue()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    SessionLogger logger;
    QSignalSpy loggingSpy(&logger, &SessionLogger::isLoggingChanged);

    QVERIFY(logger.startLogging(logPathIn(dir)));
    QVERIFY(logger.isLogging());
    QCOMPARE(loggingSpy.count(), 1);
    QVERIFY(QFile::exists(logPathIn(dir)));
}

void SessionLoggerTest::startLoggingOnAnUnwritablePathReturnsFalseAndIsLoggingStaysFalse()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString badPath = dir.filePath(QStringLiteral("no_such_subdir/session.log"));

    SessionLogger logger;
    QVERIFY(!logger.startLogging(badPath));
    QVERIFY(!logger.isLogging());
}

void SessionLoggerTest::logCommunicationWritesALineOnlyWhileLogging()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = logPathIn(dir);

    SessionLogger logger;
    // Before start: no-op, must not crash despite no open file.
    logger.logCommunication(0, QStringLiteral("Tx: 00 01"));

    QVERIFY(logger.startLogging(path));
    logger.logCommunication(1, QStringLiteral("Rx: 00 02"));
    logger.stopLogging();

    // After stop: no-op again.
    logger.logCommunication(2, QStringLiteral("should not appear"));

    const QString contents = readWholeFile(path);
    QVERIFY(contents.contains(QStringLiteral("[Rx] Rx: 00 02")));
    QVERIFY(!contents.contains(QStringLiteral("Tx: 00 01")));
    QVERIFY(!contents.contains(QStringLiteral("should not appear")));
}

void SessionLoggerTest::recordDisconnectAndRecordReconnectIncrementCountsAndWriteEventLines()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = logPathIn(dir);

    SessionLogger logger;
    QVERIFY(logger.startLogging(path));

    QSignalSpy disconnectSpy(&logger, &SessionLogger::disconnectCountChanged);
    QSignalSpy reconnectSpy(&logger, &SessionLogger::reconnectCountChanged);

    logger.recordDisconnect();
    logger.recordReconnect();
    logger.recordDisconnect();

    QCOMPARE(logger.disconnectCount(), 2);
    QCOMPARE(logger.reconnectCount(), 1);
    QCOMPARE(disconnectSpy.count(), 2);
    QCOMPARE(reconnectSpy.count(), 1);

    logger.stopLogging();
    const QString contents = readWholeFile(path);
    QVERIFY(contents.contains(QStringLiteral("disconnect #1")));
    QVERIFY(contents.contains(QStringLiteral("disconnect #2")));
    QVERIFY(contents.contains(QStringLiteral("reconnect #1")));
}

void SessionLoggerTest::stopLoggingWritesTheFooterSummaryAndClearsIsLogging()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = logPathIn(dir);

    SessionLogger logger;
    QVERIFY(logger.startLogging(path));
    logger.recordDisconnect();
    logger.recordReconnect();

    QSignalSpy loggingSpy(&logger, &SessionLogger::isLoggingChanged);
    logger.stopLogging();

    QVERIFY(!logger.isLogging());
    QCOMPARE(loggingSpy.count(), 1);

    const QString contents = readWholeFile(path);
    QVERIFY(contents.contains(QStringLiteral("1 disconnect(s)")));
    QVERIFY(contents.contains(QStringLiteral("1 reconnect(s)")));
}

void SessionLoggerTest::startingASecondLoggingSessionResetsBothCountsToZero()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    SessionLogger logger;
    QVERIFY(logger.startLogging(dir.filePath(QStringLiteral("first.log"))));
    logger.recordDisconnect();
    logger.recordDisconnect();
    logger.recordReconnect();
    logger.stopLogging();

    QVERIFY(logger.startLogging(dir.filePath(QStringLiteral("second.log"))));
    QCOMPARE(logger.disconnectCount(), 0);
    QCOMPARE(logger.reconnectCount(), 0);
}

QTEST_APPLESS_MAIN(SessionLoggerTest)
#include "test_session_logger.moc"
