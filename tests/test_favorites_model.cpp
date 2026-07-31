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
bool boolValueAt(FavoritesModel &model, int row)
{
    return model.data(model.index(row, 0), FavoritesModel::BoolValueRole).toBool();
}
bool writableAt(FavoritesModel &model, int row)
{
    return model.data(model.index(row, 0), FavoritesModel::WritableRole).toBool();
}
QList<double> historyAt(FavoritesModel &model, int row)
{
    QList<double> result;
    for (const QVariant &v : model.data(model.index(row, 0), FavoritesModel::HistoryRole).toList())
        result.append(v.toDouble());
    return result;
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

    void addFromTagCoilTagIgnoresStrayFloat32FormatAndKeepsSpanOne();
    void addAdHocCoilDefaultsToFalseBitValue();
    void applyBitUpdateSetsBoolValueAndClearsStalenessForOnlyThatRow();
    void setBitAtOnCoilEntryEmitsCoilWriteRequested();
    void setBitAtOnDiscreteInputEntryIsANoOp();
    void writableRoleMatchesRegisterTypeForAllFourTypes();

    void applyRegisterUpdateAppendsToHistory();
    void applyRegisterUpdateCapsHistoryAtTwentyPoints();
    void applyRegisterUpdateOnBitEntryLeavesHistoryEmpty();
    void setFormatAtClearsHistory();

    void applyRegisterUpdateIgnoresAResponseSizedForTheFormatBeforeALiveFormatChange();
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

// Regression test for the RegisterDefinition::registerSpan() bug: a bit-type tag
// whose FormatSettings happens to carry a stray Float32 (e.g. a malformed import
// row, since Float32 has no meaning for a 1-bit value) must still report span 1.
void FavoritesModelTest::addFromTagCoilTagIgnoresStrayFloat32FormatAndKeepsSpanOne()
{
    TagDatabaseModel tagDb;
    RegisterDefinition tag;
    tag.label = QStringLiteral("Pump Enable");
    tag.registerType = RegisterType::Coil;
    tag.address = 5;
    tag.format.format = DisplayFormat::Float32;
    tagDb.addTags({tag});

    FavoritesModel favorites;
    favorites.addFromTag(&tagDb, 0);

    const QList<PollTarget> targets = favorites.buildPollTargets(1);
    QCOMPARE(targets.size(), 1);
    QCOMPARE(int(targets.at(0).quantity), 1);
}

void FavoritesModelTest::addAdHocCoilDefaultsToFalseBitValue()
{
    FavoritesModel favorites;
    favorites.addAdHoc(int(RegisterType::Coil), 5);

    QVERIFY(favorites.data(favorites.index(0, 0), FavoritesModel::IsBitRole).toBool());
    QVERIFY(!boolValueAt(favorites, 0));
    QCOMPARE(valueAt(favorites, 0), QStringLiteral("OFF"));
}

void FavoritesModelTest::applyBitUpdateSetsBoolValueAndClearsStalenessForOnlyThatRow()
{
    FavoritesModel favorites;
    favorites.addAdHoc(int(RegisterType::Coil), 5);
    favorites.addAdHoc(int(RegisterType::Coil), 6);
    favorites.markStale(1);

    favorites.applyBitUpdate(1, 6, {true});

    QVERIFY(!boolValueAt(favorites, 0));
    QVERIFY(boolValueAt(favorites, 1));
    QVERIFY(!staleAt(favorites, 1));
}

void FavoritesModelTest::setBitAtOnCoilEntryEmitsCoilWriteRequested()
{
    FavoritesModel favorites;
    favorites.addAdHoc(int(RegisterType::Coil), 5);

    QSignalSpy coilWriteSpy(&favorites, &FavoritesModel::coilWriteRequested);
    favorites.setBitAt(0, true);

    QCOMPARE(coilWriteSpy.count(), 1);
    QCOMPARE(coilWriteSpy.first().at(0).toInt(), 5);
    QCOMPARE(coilWriteSpy.first().at(1).toBool(), true);
}

void FavoritesModelTest::setBitAtOnDiscreteInputEntryIsANoOp()
{
    FavoritesModel favorites;
    favorites.addAdHoc(int(RegisterType::DiscreteInput), 5);

    QSignalSpy coilWriteSpy(&favorites, &FavoritesModel::coilWriteRequested);
    favorites.setBitAt(0, true);

    QCOMPARE(coilWriteSpy.count(), 0);
}

void FavoritesModelTest::writableRoleMatchesRegisterTypeForAllFourTypes()
{
    FavoritesModel favorites;
    favorites.addAdHoc(int(RegisterType::Coil), 1);
    favorites.addAdHoc(int(RegisterType::DiscreteInput), 2);
    favorites.addAdHoc(int(RegisterType::HoldingRegister), 3);
    favorites.addAdHoc(int(RegisterType::InputRegister), 4);

    QVERIFY(writableAt(favorites, 0));
    QVERIFY(!writableAt(favorites, 1));
    QVERIFY(writableAt(favorites, 2));
    QVERIFY(!writableAt(favorites, 3));
}

void FavoritesModelTest::applyRegisterUpdateAppendsToHistory()
{
    FavoritesModel favorites;
    favorites.addAdHoc(int(RegisterType::HoldingRegister), 10);

    favorites.applyRegisterUpdate(0, 10, {100});
    favorites.applyRegisterUpdate(0, 10, {200});

    QCOMPARE(historyAt(favorites, 0), (QList<double>{100.0, 200.0}));
}

void FavoritesModelTest::applyRegisterUpdateCapsHistoryAtTwentyPoints()
{
    FavoritesModel favorites;
    favorites.addAdHoc(int(RegisterType::HoldingRegister), 10);

    for (int i = 0; i < 25; ++i)
        favorites.applyRegisterUpdate(0, 10, {quint16(i)});

    const QList<double> history = historyAt(favorites, 0);
    QCOMPARE(history.size(), 20);
    QCOMPARE(history.first(), 5.0); // oldest 5 (values 0..4) evicted
    QCOMPARE(history.last(), 24.0);
}

void FavoritesModelTest::applyRegisterUpdateOnBitEntryLeavesHistoryEmpty()
{
    FavoritesModel favorites;
    favorites.addAdHoc(int(RegisterType::Coil), 5);

    // Defensive path -- ConnectionController routes bit updates through
    // applyBitUpdate in practice, never this method, but the guard must hold anyway.
    favorites.applyRegisterUpdate(0, 5, {1});

    QVERIFY(historyAt(favorites, 0).isEmpty());
}

void FavoritesModelTest::setFormatAtClearsHistory()
{
    FavoritesModel favorites;
    favorites.addAdHoc(int(RegisterType::HoldingRegister), 10);
    favorites.applyRegisterUpdate(0, 10, {100});
    QVERIFY(!historyAt(favorites, 0).isEmpty());

    favorites.setFormatAt(0, int(DisplayFormat::UnsignedDecimal), int(ByteOrder::ABCD), 1.0, 0.0, QString());

    QVERIFY(historyAt(favorites, 0).isEmpty());
}

// Real crash found via GUI testing: switching a Favorites entry's format to a
// different registerSpan (e.g. Decimal -> Float32) while it's being actively
// polled resets rawValues to the new span immediately, but a response already
// in flight under the *old* span can still arrive afterward. Applying it
// unconditionally desyncs rawValues.size() from what Core::formatValue()'s
// Q_ASSERT requires for the entry's (now different) format, aborting the
// process the next time ValueRole is read. The fix: applyRegisterUpdate()
// discards a response whose size doesn't match the entry's current
// registerSpan() rather than applying it.
void FavoritesModelTest::applyRegisterUpdateIgnoresAResponseSizedForTheFormatBeforeALiveFormatChange()
{
    FavoritesModel favorites;
    favorites.addAdHoc(int(RegisterType::HoldingRegister), 10);
    favorites.applyRegisterUpdate(0, 10, {100});

    // registerSpan 1 -> 2: rawValues resets to two zeros for the new format.
    favorites.setFormatAt(0, int(DisplayFormat::Float32), int(ByteOrder::ABCD), 1.0, 0.0, QString());

    // A stale response sized for the *old* (span-1) format arrives late.
    favorites.applyRegisterUpdate(0, 10, {999});

    // Ignored, not applied -- rawValues stayed the format-consistent [0, 0]
    // (bit_cast<float>(0) == 0), and reading ValueRole doesn't hit the
    // Q_ASSERT a mismatched size would have triggered.
    QCOMPARE(valueAt(favorites, 0), QStringLiteral("0"));
}

QTEST_GUILESS_MAIN(FavoritesModelTest)
#include "test_favorites_model.moc"
