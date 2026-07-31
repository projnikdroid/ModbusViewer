#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QSettings>
#include <memory>

namespace ModbusViewer::AppLib {

// Persists which visual theme is selected, as an opaque string this class
// never interprets -- it doesn't know theme names, palettes, or how many
// themes exist. That knowledge lives entirely in app/qml/theme/Theme.qml's
// palette map, so adding/renaming a theme is a QML-only change. An empty
// themeId means "no opinion"; Theme.qml picks its own default in that case.
class ThemeSettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString themeId READ themeId WRITE setThemeId NOTIFY themeIdChanged)

public:
    // settingsFilePath empty (the default) uses the process-wide default
    // QSettings() location/format set up in main.cpp; a non-empty path
    // (Ini format) is the test-only injection seam so tests never touch the
    // developer's real config.
    explicit ThemeSettings(const QString &settingsFilePath = QString(), QObject *parent = nullptr);

    QString themeId() const;
    void setThemeId(const QString &themeId);

signals:
    void themeIdChanged();

private:
    std::unique_ptr<QSettings> m_settings;
    QString m_themeId;
};

} // namespace ModbusViewer::AppLib
