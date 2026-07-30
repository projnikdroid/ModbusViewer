#include <QSignalSpy>
#include <QTest>

#include "format/ValueFormatter.h"
#include "models/RegisterTableModel.h"

using namespace ModbusViewer::AppLib;
using ModbusViewer::Core::DisplayFormat;
using ModbusViewer::Core::ByteOrder;

class RegisterTableModelTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultFormatMatchesPlainDecimalDisplay();
    void settingFloat32MergesTwoRowsIntoOne();
    void fallbackWhenNoNextRegisterAvailable();
    void addressConventionTogglesDisplayWithoutChangingWireAddress();
    void setValueAtOnMergedRowEmitsWriteRequestedTwice();
    void sameShapeUpdateStillEmitsSingleBatchedDataChanged();
    void markStaleSetsStaleAndEmitsStaleChanged();
    void setRegistersClearsStaleness();

    void settingRegisterTypeResetsExistingData();
    void setBitsPopulatesBoolValueRole();
    void addressRoleUsesTheSelectedRegisterTypesModiconPrefix();
    void sameShapeBitUpdateStillEmitsSingleBatchedDataChanged();
    void setBitAtOnCoilRowEmitsCoilWriteRequested();
    void setBitAtOnDiscreteInputRowIsANoOp();
};

namespace {
QString addressAt(RegisterTableModel &model, int row)
{
    return model.data(model.index(row, 0), RegisterTableModel::AddressRole).toString();
}
QString valueAt(RegisterTableModel &model, int row)
{
    return model.data(model.index(row, 0), RegisterTableModel::ValueRole).toString();
}
} // namespace

void RegisterTableModelTest::defaultFormatMatchesPlainDecimalDisplay()
{
    RegisterTableModel model;
    model.setRegisters(0, {100, 200});

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(addressAt(model, 0), QStringLiteral("0"));
    QCOMPARE(valueAt(model, 0), QStringLiteral("100"));
    QCOMPARE(addressAt(model, 1), QStringLiteral("1"));
    QCOMPARE(valueAt(model, 1), QStringLiteral("200"));
}

void RegisterTableModelTest::settingFloat32MergesTwoRowsIntoOne()
{
    RegisterTableModel model;
    // 1.5f == 0x3FC00000, ABCD byte order.
    model.setRegisters(0, {0x3FC0, 0x0000, 999});
    QCOMPARE(model.rowCount(), 3);

    model.setFormatAt(0, int(DisplayFormat::Float32), int(ByteOrder::ABCD), 1.0, 0.0, QString());

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(addressAt(model, 0), QStringLiteral("0-1"));
    QCOMPARE(valueAt(model, 0), QStringLiteral("1.5"));
    QCOMPARE(addressAt(model, 1), QStringLiteral("2"));
    QCOMPARE(valueAt(model, 1), QStringLiteral("999"));
}

void RegisterTableModelTest::fallbackWhenNoNextRegisterAvailable()
{
    RegisterTableModel model;
    model.setRegisters(0, {100, 0x3FC0});

    // Row 1 is the last register -- no partner register 2 exists to pair with.
    model.setFormatAt(1, int(DisplayFormat::Float32), int(ByteOrder::ABCD), 1.0, 0.0, QString());

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(addressAt(model, 1), QStringLiteral("1"));
    QCOMPARE(valueAt(model, 1), QStringLiteral("16320")); // degrades to plain unsigned decimal

    // The requested format is remembered even though it can't currently be honored.
    const QVariantMap settings = model.formatSettingsAt(1);
    QCOMPARE(settings.value("format").toInt(), int(DisplayFormat::Float32));
}

void RegisterTableModelTest::addressConventionTogglesDisplayWithoutChangingWireAddress()
{
    RegisterTableModel model;
    model.setRegisters(0, {100});

    QCOMPARE(addressAt(model, 0), QStringLiteral("0"));

    model.setAddressConvention(RegisterTableModel::AddressConvention::Modicon);
    QCOMPARE(addressAt(model, 0), QStringLiteral("40001"));

    QSignalSpy writeSpy(&model, &RegisterTableModel::writeRequested);
    model.setValueAt(0, QStringLiteral("123"));
    QCOMPARE(writeSpy.count(), 1);
    QCOMPARE(writeSpy.at(0).at(0).toInt(), 0); // still the raw PDU address
    QCOMPARE(writeSpy.at(0).at(1).toInt(), 123);
}

void RegisterTableModelTest::setValueAtOnMergedRowEmitsWriteRequestedTwice()
{
    RegisterTableModel model;
    model.setRegisters(0, {0, 0});
    model.setFormatAt(0, int(DisplayFormat::Float32), int(ByteOrder::ABCD), 1.0, 0.0, QString());

    QSignalSpy writeSpy(&model, &RegisterTableModel::writeRequested);
    model.setValueAt(0, QStringLiteral("1.5"));

    QCOMPARE(writeSpy.count(), 2);
    QCOMPARE(writeSpy.at(0).at(0).toInt(), 0);
    QCOMPARE(writeSpy.at(0).at(1).toInt(), 0x3FC0);
    QCOMPARE(writeSpy.at(1).at(0).toInt(), 1);
    QCOMPARE(writeSpy.at(1).at(1).toInt(), 0x0000);
}

void RegisterTableModelTest::sameShapeUpdateStillEmitsSingleBatchedDataChanged()
{
    RegisterTableModel model;
    model.setRegisters(0, {1, 2, 3, 4, 5});

    QSignalSpy spy(&model, &RegisterTableModel::dataChanged);
    model.setRegisters(0, {1, 99, 3, 99, 5});

    QCOMPARE(spy.count(), 1);
    const QModelIndex topLeft = spy.at(0).at(0).toModelIndex();
    const QModelIndex bottomRight = spy.at(0).at(1).toModelIndex();
    QCOMPARE(topLeft.row(), 1);
    QCOMPARE(bottomRight.row(), 3);
}

void RegisterTableModelTest::markStaleSetsStaleAndEmitsStaleChanged()
{
    RegisterTableModel model;
    QVERIFY(!model.stale());

    QSignalSpy staleSpy(&model, &RegisterTableModel::staleChanged);
    model.markStale();

    QVERIFY(model.stale());
    QCOMPARE(staleSpy.count(), 1);

    // Marking an already-stale model again must not re-emit.
    model.markStale();
    QCOMPARE(staleSpy.count(), 1);
}

void RegisterTableModelTest::setRegistersClearsStaleness()
{
    RegisterTableModel model;
    model.setRegisters(0, {100, 200});
    model.markStale();
    QVERIFY(model.stale());

    QSignalSpy staleSpy(&model, &RegisterTableModel::staleChanged);
    model.setRegisters(0, {101, 201});

    QVERIFY(!model.stale());
    QCOMPARE(staleSpy.count(), 1);
}

void RegisterTableModelTest::settingRegisterTypeResetsExistingData()
{
    RegisterTableModel model;
    model.setRegisters(0, {100, 200});
    QCOMPARE(model.rowCount(), 2);

    model.setRegisterType(RegisterTableModel::RegisterType::Coil);

    QCOMPARE(model.rowCount(), 0);
}

void RegisterTableModelTest::setBitsPopulatesBoolValueRole()
{
    RegisterTableModel model;
    model.setRegisterType(RegisterTableModel::RegisterType::Coil);
    model.setBits(0, {true, false, true});

    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(valueAt(model, 0), QStringLiteral("ON"));
    QCOMPARE(valueAt(model, 1), QStringLiteral("OFF"));
    QVERIFY(model.data(model.index(0, 0), RegisterTableModel::BoolValueRole).toBool());
    QVERIFY(!model.data(model.index(1, 0), RegisterTableModel::BoolValueRole).toBool());
    QVERIFY(model.data(model.index(0, 0), RegisterTableModel::IsBitRole).toBool());
}

void RegisterTableModelTest::addressRoleUsesTheSelectedRegisterTypesModiconPrefix()
{
    RegisterTableModel model;
    model.setAddressConvention(RegisterTableModel::AddressConvention::Modicon);

    model.setRegisterType(RegisterTableModel::RegisterType::DiscreteInput);
    model.setBits(0, {false});
    QCOMPARE(addressAt(model, 0), QStringLiteral("10001"));

    model.setRegisterType(RegisterTableModel::RegisterType::HoldingRegister);
    model.setRegisters(0, {0});
    QCOMPARE(addressAt(model, 0), QStringLiteral("40001"));
}

void RegisterTableModelTest::sameShapeBitUpdateStillEmitsSingleBatchedDataChanged()
{
    RegisterTableModel model;
    model.setRegisterType(RegisterTableModel::RegisterType::Coil);
    model.setBits(0, {false, false, false, false, false});

    QSignalSpy spy(&model, &RegisterTableModel::dataChanged);
    model.setBits(0, {false, true, false, true, false});

    QCOMPARE(spy.count(), 1);
    const QModelIndex topLeft = spy.at(0).at(0).toModelIndex();
    const QModelIndex bottomRight = spy.at(0).at(1).toModelIndex();
    QCOMPARE(topLeft.row(), 1);
    QCOMPARE(bottomRight.row(), 3);
}

void RegisterTableModelTest::setBitAtOnCoilRowEmitsCoilWriteRequested()
{
    RegisterTableModel model;
    model.setRegisterType(RegisterTableModel::RegisterType::Coil);
    model.setBits(5, {false});

    QSignalSpy coilWriteSpy(&model, &RegisterTableModel::coilWriteRequested);
    model.setBitAt(0, true);

    QCOMPARE(coilWriteSpy.count(), 1);
    QCOMPARE(coilWriteSpy.first().at(0).toInt(), 5);
    QCOMPARE(coilWriteSpy.first().at(1).toBool(), true);
}

void RegisterTableModelTest::setBitAtOnDiscreteInputRowIsANoOp()
{
    RegisterTableModel model;
    model.setRegisterType(RegisterTableModel::RegisterType::DiscreteInput);
    model.setBits(5, {false});

    QSignalSpy coilWriteSpy(&model, &RegisterTableModel::coilWriteRequested);
    model.setBitAt(0, true);

    QCOMPARE(coilWriteSpy.count(), 0);
}

QTEST_GUILESS_MAIN(RegisterTableModelTest)
#include "test_register_table_model.moc"
