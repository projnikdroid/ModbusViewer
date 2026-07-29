#include <QDir>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QTest>

#include "TagDatabaseController.h"
#include "models/TagDatabaseModel.h"

using namespace ModbusViewer::AppLib;

class TagDatabaseControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void importCsvPopulatesTheGivenModel();
    void importJsonPopulatesTheGivenModel();
    void missingFileEmitsAnErrorAndImportsNothing();
};

void TagDatabaseControllerTest::importCsvPopulatesTheGivenModel()
{
    QTemporaryFile file(QDir::tempPath() + QStringLiteral("/tagdb_test_XXXXXX.csv"));
    QVERIFY(file.open());
    file.write("label,registerType,address\nTank,HoldingRegister,10\n");
    file.close();

    TagDatabaseController controller;
    TagDatabaseModel model;
    QSignalSpy spy(&controller, &TagDatabaseController::importFinished);

    controller.importCsv(file.fileName(), &model);

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toInt(), 1);
    QVERIFY(spy.at(0).at(1).toStringList().isEmpty());
}

void TagDatabaseControllerTest::importJsonPopulatesTheGivenModel()
{
    QTemporaryFile file(QDir::tempPath() + QStringLiteral("/tagdb_test_XXXXXX.json"));
    QVERIFY(file.open());
    file.write(R"([{ "label": "Tank", "registerType": "HoldingRegister", "address": 10 }])");
    file.close();

    TagDatabaseController controller;
    TagDatabaseModel model;
    QSignalSpy spy(&controller, &TagDatabaseController::importFinished);

    controller.importJson(file.fileName(), &model);

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toInt(), 1);
}

void TagDatabaseControllerTest::missingFileEmitsAnErrorAndImportsNothing()
{
    TagDatabaseController controller;
    TagDatabaseModel model;
    QSignalSpy spy(&controller, &TagDatabaseController::importFinished);

    controller.importCsv(QStringLiteral("C:/does/not/exist.csv"), &model);

    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toInt(), 0);
    QVERIFY(!spy.at(0).at(1).toStringList().isEmpty());
}

QTEST_GUILESS_MAIN(TagDatabaseControllerTest)
#include "test_tag_database_controller.moc"
