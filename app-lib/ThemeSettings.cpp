#include "ThemeSettings.h"

namespace ModbusViewer::AppLib {

namespace {
const QString kThemeIdKey = QStringLiteral("ui/themeId");
}

ThemeSettings::ThemeSettings(const QString &settingsFilePath, QObject *parent)
    : QObject(parent)
    , m_settings(settingsFilePath.isEmpty() ? std::make_unique<QSettings>()
                                             : std::make_unique<QSettings>(settingsFilePath, QSettings::IniFormat))
    , m_themeId(m_settings->value(kThemeIdKey, QString()).toString())
{
}

QString ThemeSettings::themeId() const
{
    return m_themeId;
}

void ThemeSettings::setThemeId(const QString &themeId)
{
    if (m_themeId == themeId)
        return;
    m_themeId = themeId;
    m_settings->setValue(kThemeIdKey, m_themeId);
    emit themeIdChanged();
}

} // namespace ModbusViewer::AppLib
