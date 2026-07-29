#pragma once

#include <QObject>
#include <QQmlEngine>

namespace ModbusViewer::AppLib {

// Which target set the single shared PollEngine is currently polling: the Normal
// view's contiguous address range, or the scattered FavoritesModel entries. Exactly
// one at a time (see docs/favorites-search-tags.md, "one mode toggle, one
// PollEngine"). Deliberately just a mode flag -- mode switching itself is
// coordinated at the QML layer (MainScreen.qml calls ConnectionController's
// startPolling/startPollingFavorites in response to mode changing), matching this
// codebase's existing idiom for cross-singleton coordination via bindings rather
// than direct C++ coupling between singletons.
class PollModeController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(Mode mode READ mode WRITE setMode NOTIFY modeChanged)

public:
    enum class Mode { Normal, Favorites };
    Q_ENUM(Mode)

    explicit PollModeController(QObject *parent = nullptr);

    Mode mode() const;
    void setMode(Mode mode);

signals:
    void modeChanged();

private:
    Mode m_mode = Mode::Normal;
};

} // namespace ModbusViewer::AppLib
