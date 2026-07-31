#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "ThemeSettings.h"

using namespace ModbusViewer::AppLib;

namespace {
QString iniPathIn(const QTemporaryDir &dir)
{
    return dir.filePath(QStringLiteral("theme.ini"));
}
} // namespace

class ThemeSettingsTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultThemeIdIsEmptyOnFreshSettingsFile();
    void setThemeIdPersistsAcrossInstances();
    void setThemeIdToEmptyStringPersistsAsEmpty();
    void setThemeIdAcceptsAnArbitraryUnrecognizedId();
    void setThemeIdEmitsChangedSignalOnlyWhenValueActuallyChanges();
};

// Empty means "no opinion" -- Theme.qml (not this class) decides what the
// actual default palette is when themeId is empty/unrecognized.
void ThemeSettingsTest::defaultThemeIdIsEmptyOnFreshSettingsFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    ThemeSettings settings(iniPathIn(dir));
    QCOMPARE(settings.themeId(), QString());
}

void ThemeSettingsTest::setThemeIdPersistsAcrossInstances()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    {
        ThemeSettings writer(iniPathIn(dir));
        writer.setThemeId(QStringLiteral("signalConsole"));
    }

    ThemeSettings reader(iniPathIn(dir));
    QCOMPARE(reader.themeId(), QStringLiteral("signalConsole"));
}

void ThemeSettingsTest::setThemeIdToEmptyStringPersistsAsEmpty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    {
        ThemeSettings writer(iniPathIn(dir));
        writer.setThemeId(QStringLiteral("glassHud"));
        writer.setThemeId(QString());
    }

    ThemeSettings reader(iniPathIn(dir));
    QCOMPARE(reader.themeId(), QString());
}

// The backend must not validate against a known set of theme names -- that
// knowledge lives entirely in Theme.qml's palette map, per the plan's
// frontend/backend decoupling requirement.
void ThemeSettingsTest::setThemeIdAcceptsAnArbitraryUnrecognizedId()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    {
        ThemeSettings writer(iniPathIn(dir));
        writer.setThemeId(QStringLiteral("nonsenseTheme123"));
    }

    ThemeSettings reader(iniPathIn(dir));
    QCOMPARE(reader.themeId(), QStringLiteral("nonsenseTheme123"));
}

void ThemeSettingsTest::setThemeIdEmitsChangedSignalOnlyWhenValueActuallyChanges()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    ThemeSettings settings(iniPathIn(dir));
    QSignalSpy spy(&settings, &ThemeSettings::themeIdChanged);

    settings.setThemeId(QStringLiteral("glassHud"));
    QCOMPARE(spy.count(), 1);

    settings.setThemeId(QStringLiteral("glassHud"));
    QCOMPARE(spy.count(), 1);

    settings.setThemeId(QStringLiteral("signalConsole"));
    QCOMPARE(spy.count(), 2);
}

QTEST_APPLESS_MAIN(ThemeSettingsTest)
#include "test_theme_settings.moc"
