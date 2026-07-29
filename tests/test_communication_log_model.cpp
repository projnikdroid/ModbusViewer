#include <QTest>

#include "CommunicationLogModel.h"

using namespace ModbusViewer::AppLib;

namespace {
QString summaryAt(CommunicationLogModel &model, int row)
{
    return model.data(model.index(row, 0), CommunicationLogModel::SummaryRole).toString();
}
int directionAt(CommunicationLogModel &model, int row)
{
    return model.data(model.index(row, 0), CommunicationLogModel::DirectionRole).toInt();
}
} // namespace

class CommunicationLogModelTest : public QObject
{
    Q_OBJECT

private slots:
    void appendAddsAnEntryWithDirectionAndSummary();
    void ringBufferEvictsOldestEntryOnceAtCapacity();
    void clearEmptiesTheModel();
};

void CommunicationLogModelTest::appendAddsAnEntryWithDirectionAndSummary()
{
    CommunicationLogModel model;
    model.append(int(CommunicationLogModel::Direction::Tx), QStringLiteral("Tx: 01 03 00 00 00 01"));

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(directionAt(model, 0), int(CommunicationLogModel::Direction::Tx));
    QCOMPARE(summaryAt(model, 0), QStringLiteral("Tx: 01 03 00 00 00 01"));

    const QVariant timestamp = model.data(model.index(0, 0), CommunicationLogModel::TimestampRole);
    QVERIFY(!timestamp.toString().isEmpty());
}

void CommunicationLogModelTest::ringBufferEvictsOldestEntryOnceAtCapacity()
{
    CommunicationLogModel model;
    for (int i = 0; i < CommunicationLogModel::MaxEntries + 1; ++i)
        model.append(int(CommunicationLogModel::Direction::Tx), QStringLiteral("entry-%1").arg(i));

    QCOMPARE(model.rowCount(), CommunicationLogModel::MaxEntries);
    // entry-0 was evicted; entry-1 is now the oldest surviving row.
    QCOMPARE(summaryAt(model, 0), QStringLiteral("entry-1"));
    QCOMPARE(summaryAt(model, CommunicationLogModel::MaxEntries - 1),
             QStringLiteral("entry-%1").arg(CommunicationLogModel::MaxEntries));
}

void CommunicationLogModelTest::clearEmptiesTheModel()
{
    CommunicationLogModel model;
    model.append(int(CommunicationLogModel::Direction::Rx), QStringLiteral("Rx: 01 03 02 00 64"));
    QCOMPARE(model.rowCount(), 1);

    model.clear();
    QCOMPARE(model.rowCount(), 0);
}

QTEST_GUILESS_MAIN(CommunicationLogModelTest)
#include "test_communication_log_model.moc"
