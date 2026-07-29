#include <QTest>

#include "models/FavoritesModel.h"
#include "models/RegisterFilterProxyModel.h"
#include "models/RegisterTableModel.h"
#include "models/TagDatabaseModel.h"

using namespace ModbusViewer::AppLib;
using ModbusViewer::Core::RegisterType;

namespace {
TagDatabaseModel *makeTagDatabase(QObject *parent)
{
    auto *db = new TagDatabaseModel(parent);
    ModbusViewer::Core::RegisterDefinition tankLevel;
    tankLevel.label = QStringLiteral("Tank Level");
    tankLevel.description = QStringLiteral("North tank sensor");
    tankLevel.address = 100;
    tankLevel.format.unit = QStringLiteral("m");
    ModbusViewer::Core::RegisterDefinition pumpSpeed;
    pumpSpeed.label = QStringLiteral("Pump Speed");
    pumpSpeed.description = QStringLiteral("Main pump");
    pumpSpeed.address = 200;
    pumpSpeed.format.unit = QStringLiteral("rpm");
    db->addTags({tankLevel, pumpSpeed});
    return db;
}
} // namespace

class RegisterFilterProxyModelTest : public QObject
{
    Q_OBJECT

private slots:
    void filtersByLabelSubstringCaseInsensitive();
    void filtersByDescriptionSubstring();
    void filtersByUnitSubstring();
    void filtersByAddress();
    void emptyFilterTextAcceptsEverything();
    void rowMatchingNothingIsExcluded();
    void worksAgainstSourceModelMissingLabelRoles();
    void mapRowToSourceAccountsForHiddenPrecedingRows();
    void valueOnlyUpdateDoesNotDropAMatchingRow();
};

void RegisterFilterProxyModelTest::filtersByLabelSubstringCaseInsensitive()
{
    TagDatabaseModel *db = makeTagDatabase(this);
    RegisterFilterProxyModel proxy;
    proxy.setSourceModel(db);

    proxy.setFilterText(QStringLiteral("tank"));
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.data(proxy.index(0, 0), TagDatabaseModel::LabelRole).toString(), QStringLiteral("Tank Level"));
}

void RegisterFilterProxyModelTest::filtersByDescriptionSubstring()
{
    TagDatabaseModel *db = makeTagDatabase(this);
    RegisterFilterProxyModel proxy;
    proxy.setSourceModel(db);

    proxy.setFilterText(QStringLiteral("main pump"));
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.data(proxy.index(0, 0), TagDatabaseModel::LabelRole).toString(), QStringLiteral("Pump Speed"));
}

void RegisterFilterProxyModelTest::filtersByUnitSubstring()
{
    TagDatabaseModel *db = makeTagDatabase(this);
    RegisterFilterProxyModel proxy;
    proxy.setSourceModel(db);

    proxy.setFilterText(QStringLiteral("rpm"));
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.data(proxy.index(0, 0), TagDatabaseModel::LabelRole).toString(), QStringLiteral("Pump Speed"));
}

void RegisterFilterProxyModelTest::filtersByAddress()
{
    TagDatabaseModel *db = makeTagDatabase(this);
    RegisterFilterProxyModel proxy;
    proxy.setSourceModel(db);

    proxy.setFilterText(QStringLiteral("100"));
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.data(proxy.index(0, 0), TagDatabaseModel::LabelRole).toString(), QStringLiteral("Tank Level"));
}

void RegisterFilterProxyModelTest::emptyFilterTextAcceptsEverything()
{
    TagDatabaseModel *db = makeTagDatabase(this);
    RegisterFilterProxyModel proxy;
    proxy.setSourceModel(db);

    QCOMPARE(proxy.rowCount(), 2);
}

void RegisterFilterProxyModelTest::rowMatchingNothingIsExcluded()
{
    TagDatabaseModel *db = makeTagDatabase(this);
    RegisterFilterProxyModel proxy;
    proxy.setSourceModel(db);

    proxy.setFilterText(QStringLiteral("nonexistent"));
    QCOMPARE(proxy.rowCount(), 0);
}

void RegisterFilterProxyModelTest::worksAgainstSourceModelMissingLabelRoles()
{
    RegisterTableModel registerModel;
    registerModel.setRegisters(100, {10, 20, 30});

    RegisterFilterProxyModel proxy;
    proxy.setSourceModel(&registerModel);

    proxy.setFilterText(QStringLiteral("101"));
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.data(proxy.index(0, 0), RegisterTableModel::AddressRole).toString(), QStringLiteral("101"));
}

void RegisterFilterProxyModelTest::mapRowToSourceAccountsForHiddenPrecedingRows()
{
    FavoritesModel favorites;
    favorites.addAdHoc(int(RegisterType::HoldingRegister), 7);  // label "7", source row 0
    favorites.addAdHoc(int(RegisterType::HoldingRegister), 99); // label "99", source row 1 -- filtered out
    favorites.addAdHoc(int(RegisterType::HoldingRegister), 17); // label "17", source row 2

    RegisterFilterProxyModel proxy;
    proxy.setSourceModel(&favorites);
    proxy.setFilterText(QStringLiteral("7")); // matches "7" and "17", not "99"

    QCOMPARE(proxy.rowCount(), 2);
    QCOMPARE(proxy.mapRowToSource(0), 0); // "7"
    QCOMPARE(proxy.mapRowToSource(1), 2); // "17" -- source row 1 ("99") is hidden
}

void RegisterFilterProxyModelTest::valueOnlyUpdateDoesNotDropAMatchingRow()
{
    FavoritesModel favorites;
    favorites.addAdHoc(int(RegisterType::HoldingRegister), 42);

    RegisterFilterProxyModel proxy;
    proxy.setSourceModel(&favorites);
    proxy.setFilterText(QStringLiteral("42"));
    QCOMPARE(proxy.rowCount(), 1);

    favorites.applyRegisterUpdate(0, 42, {123});

    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.data(proxy.index(0, 0), FavoritesModel::ValueRole).toString(), QStringLiteral("123"));
}

QTEST_GUILESS_MAIN(RegisterFilterProxyModelTest)
#include "test_register_filter_proxy_model.moc"
