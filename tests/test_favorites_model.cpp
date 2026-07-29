#include <QSignalSpy>
#include <QTest>

#include "models/FavoritesModel.h"
#include "models/TagDatabaseModel.h"

using namespace ModbusViewer::AppLib;
using ModbusViewer::Core::ByteOrder;
using ModbusViewer::Core::DisplayFormat;
using ModbusViewer::Core::PollTarget;
using ModbusViewer::Core::RegisterDefinition;
using ModbusViewer::Core::RegisterType;
using ModbusViewer::Core::TagSource;

namespace {
QString labelAt(FavoritesModel &model, int row)
{
    return model.data(model.index(row, 0), FavoritesModel::LabelRole).toString();
}
QString addressAt(FavoritesModel &model, int row)
{
    return model.data(model.index(row, 0), FavoritesModel::AddressRole).toString();
}
QString valueAt(FavoritesModel &model, int row)
{
    return model.data(model.index(row, 0), FavoritesModel::ValueRole).toString();
}
bool staleAt(FavoritesModel &model, int row)
{
    return model.data(model.index(row, 0), FavoritesModel::StaleRole).toBool();
}
} // namespace

class FavoritesModelTest : public QObject
{
    Q_OBJECT

private slots:
    void addFromTagCopiesFieldsAndZeroFillsSpan();
    void addAdHocUsesAddressAsLabelAndRawFormat();
    void buildPollTargetsMatchesEntryOrderAndSpan();
    void applyRegisterUpdateOnlyChangesTargetedRow();
    void setValueAtOnMergedRowEmitsWriteRequestedTwice();
    void removeAtRemovesTheEntry();
    void markStaleSetsStaleRoleForOnlyThatRow();
    void applyRegisterUpdateClearsStaleness();
};

void FavoritesModelTest::addFromTagCopiesFieldsAndZeroFillsSpan()
{
    TagDatabaseModel tagDb;
    RegisterDefinition tag;
    tag.label = QStringLiteral("Tank Level");
    tag.description = QStringLiteral("North tank");
    tag.registerType = RegisterType::HoldingRegister;
    tag.address = 100;
    tag.format.format = DisplayFormat::Float32;
    tag.format.unit = QStringLiteral("m");
    tagDb.addTags({tag});

    FavoritesModel favorites;
    favorites.addFromTag(&tagDb, 0);

    QCOMPARE(favorites.rowCount(), 1);
    QCOMPARE(labelAt(favorites, 0), QStringLiteral("Tank Level"));
    // Zero-filled Float32 formats as 0, with the tag's unit suffix appended.
    QCOMPARE(valueAt(favorites, 0), QStringLiteral("0 m"));

    const QList<PollTarget> targets = favorites.buildPollTargets(1);
    QCOMPARE(targets.size(), 1);
    QCOMPARE(int(targets.at(0).quantity), 2); // Float32 spans two registers
}

void FavoritesModelTest::addAdHocUsesAddressAsLabelAndRawFormat()
{
    FavoritesModel favorites;
    favorites.addAdHoc(int(RegisterType::HoldingRegister), 42);

    QCOMPARE(favorites.rowCount(), 1);
    QCOMPARE(labelAt(favorites, 0), QStringLiteral("42"));

    const QVariantMap settings = favorites.formatSettingsAt(0);
    QCOMPARE(settings.value("format").toInt(), int(DisplayFormat::UnsignedDecimal));

    const QList<PollTarget> targets = favorites.buildPollTargets(1);
    QCOMPARE(targets.size(), 1);
    QCOMPARE(int(targets.at(0).quantity), 1);
    QCOMPARE(int(targets.at(0).startAddress), 42);
    QCOMPARE(targets.at(0).registerType, RegisterType::HoldingRegister);
}

void FavoritesModelTest::buildPollTargetsMatchesEntryOrderAndSpan()
{
    FavoritesModel favorites;
    favorites.addAdHoc(int(RegisterType::HoldingRegister), 10);
    favorites.addAdHoc(int(RegisterType::InputRegister), 20);

    const QList<PollTarget> targets = favorites.buildPollTargets(7);
    QCOMPARE(targets.size(), 2);
    QCOMPARE(int(targets.at(0).startAddress), 10);
    QCOMPARE(targets.at(0).registerType, RegisterType::HoldingRegister);
    QCOMPARE(int(targets.at(0).unitId), 7);
    QCOMPARE(int(targets.at(1).startAddress), 20);
    QCOMPARE(targets.at(1).registerType, RegisterType::InputRegister);
    QCOMPARE(int(targets.at(1).unitId), 7);
}

void FavoritesModelTest::applyRegisterUpdateOnlyChangesTargetedRow()
{
    FavoritesModel favorites;
    favorites.addAdHoc(int(RegisterType::HoldingRegister), 10);
    favorites.addAdHoc(int(RegisterType::HoldingRegister), 20);

    favorites.applyRegisterUpdate(1, 20, {123});

    QCOMPARE(valueAt(favorites, 0), QStringLiteral("0"));
    QCOMPARE(valueAt(favorites, 1), QStringLiteral("123"));
}

void FavoritesModelTest::setValueAtOnMergedRowEmitsWriteRequestedTwice()
{
    TagDatabaseModel tagDb;
    RegisterDefinition tag;
    tag.address = 0;
    tag.registerType = RegisterType::HoldingRegister;
    tag.format.format = DisplayFormat::Float32;
    tag.format.byteOrder = ByteOrder::ABCD;
    tagDb.addTags({tag});

    FavoritesModel favorites;
    favorites.addFromTag(&tagDb, 0);

    QSignalSpy writeSpy(&favorites, &FavoritesModel::writeRequested);
    favorites.setValueAt(0, QStringLiteral("1.5"));

    QCOMPARE(writeSpy.count(), 2);
    QCOMPARE(writeSpy.at(0).at(0).toInt(), 0);
    QCOMPARE(writeSpy.at(0).at(1).toInt(), 0x3FC0);
    QCOMPARE(writeSpy.at(1).at(0).toInt(), 1);
    QCOMPARE(writeSpy.at(1).at(1).toInt(), 0x0000);
}

void FavoritesModelTest::removeAtRemovesTheEntry()
{
    FavoritesModel favorites;
    favorites.addAdHoc(int(RegisterType::HoldingRegister), 10);
    favorites.addAdHoc(int(RegisterType::HoldingRegister), 20);

    favorites.removeAt(0);

    QCOMPARE(favorites.rowCount(), 1);
    QCOMPARE(addressAt(favorites, 0), QStringLiteral("20"));
}

void FavoritesModelTest::markStaleSetsStaleRoleForOnlyThatRow()
{
    FavoritesModel favorites;
    favorites.addAdHoc(int(RegisterType::HoldingRegister), 10);
    favorites.addAdHoc(int(RegisterType::HoldingRegister), 20);

    QSignalSpy dataChangedSpy(&favorites, &FavoritesModel::dataChanged);
    favorites.markStale(1);

    QVERIFY(!staleAt(favorites, 0));
    QVERIFY(staleAt(favorites, 1));
    QCOMPARE(dataChangedSpy.count(), 1);

    // Marking an already-stale row again must not re-emit.
    favorites.markStale(1);
    QCOMPARE(dataChangedSpy.count(), 1);
}

void FavoritesModelTest::applyRegisterUpdateClearsStaleness()
{
    FavoritesModel favorites;
    favorites.addAdHoc(int(RegisterType::HoldingRegister), 10);
    favorites.markStale(0);
    QVERIFY(staleAt(favorites, 0));

    favorites.applyRegisterUpdate(0, 10, {123});

    QVERIFY(!staleAt(favorites, 0));
}

QTEST_GUILESS_MAIN(FavoritesModelTest)
#include "test_favorites_model.moc"
