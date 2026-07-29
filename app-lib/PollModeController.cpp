#include "PollModeController.h"

namespace ModbusViewer::AppLib {

PollModeController::PollModeController(QObject *parent)
    : QObject(parent)
{
}

PollModeController::Mode PollModeController::mode() const
{
    return m_mode;
}

void PollModeController::setMode(Mode mode)
{
    if (m_mode == mode)
        return;
    m_mode = mode;
    emit modeChanged();
}

} // namespace ModbusViewer::AppLib
