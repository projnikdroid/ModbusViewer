#include <QTest>

#include "models/TagDatabaseModel.h"

using namespace ModbusViewer::AppLib;
using ModbusViewer::Core::RegisterDefinition;
using ModbusViewer::Core::RegisterType;
using ModbusViewer::Core::DisplayFormat;

namespace {
RegisterDefinition makeTag(const QString &label, int address)
{
    RegisterDefinition tag;
    tag.label = label;
    tag.address = address;
    tag.registerType = RegisterType::HoldingRegister;
    return tag;
}
} // namespace

class TagDatabaseModelTest : public QObject
{
    Q_OBJECT

private slots:
    void addTagsAppendsAndExposesRoles();
    void multipleImportsAccumulateRatherThanReplace();
    void clearEmptiesTheModel();
};

void TagDatabaseModelTest::addTagsAppendsAndExposesRoles()
{
    TagDatabaseModel model;
    RegisterDefinition tag = makeTag(QStringLiteral("Tank Level"), 100);
    tag.description = QStringLiteral("North tank");
    tag.format.format = DisplayFormat::Float32;
    tag.format.unit = QStringLiteral("m");

    model.addTags({tag});

    QCOMPARE(model.rowCount(), 1);
    const QModelIndex idx = model.index(0, 0);
    QCOMPARE(model.data(idx, TagDatabaseModel::LabelRole).toString(), QStringLiteral("Tank Level"));
    QCOMPARE(model.data(idx, TagDatabaseModel::DescriptionRole).toString(), QStringLiteral("North tank"));
    QCOMPARE(model.data(idx, TagDatabaseModel::AddressRole).toInt(), 100);
    QCOMPARE(model.data(idx, TagDatabaseModel::FormatRole).toInt(), int(DisplayFormat::Float32));
    QCOMPARE(model.data(idx, TagDatabaseModel::UnitRole).toString(), QStringLiteral("m"));
}

void TagDatabaseModelTest::multipleImportsAccumulateRatherThanReplace()
{
    TagDatabaseModel model;
    model.addTags({makeTag(QStringLiteral("First"), 1)});
    model.addTags({makeTag(QStringLiteral("Second"), 2), makeTag(QStringLiteral("Third"), 3)});

    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.data(model.index(0, 0), TagDatabaseModel::LabelRole).toString(), QStringLiteral("First"));
    QCOMPARE(model.data(model.index(2, 0), TagDatabaseModel::LabelRole).toString(), QStringLiteral("Third"));
}

void TagDatabaseModelTest::clearEmptiesTheModel()
{
    TagDatabaseModel model;
    model.addTags({makeTag(QStringLiteral("First"), 1)});
    QCOMPARE(model.rowCount(), 1);

    model.clear();
    QCOMPARE(model.rowCount(), 0);
}

QTEST_GUILESS_MAIN(TagDatabaseModelTest)
#include "test_tag_database_model.moc"
