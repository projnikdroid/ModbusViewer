#include "DisplaySettings.h"

#include "format/AddressConvention.h"
#include "model/RegisterType.h"

namespace ModbusViewer::AppLib {

namespace {

Core::AddressConvention toCoreConvention(DisplaySettings::AddressConvention convention)
{
    return convention == DisplaySettings::AddressConvention::Modicon ? Core::AddressConvention::Modicon
                                                                      : Core::AddressConvention::Pdu;
}

} // namespace

DisplaySettings::DisplaySettings(QObject *parent)
    : QObject(parent)
{
}

DisplaySettings::AddressConvention DisplaySettings::addressConvention() const
{
    return m_addressConvention;
}

void DisplaySettings::setAddressConvention(AddressConvention convention)
{
    if (m_addressConvention == convention)
        return;
    m_addressConvention = convention;
    emit addressConventionChanged();
}

bool DisplaySettings::flashOnUpdateEnabled() const
{
    return m_flashOnUpdateEnabled;
}

void DisplaySettings::setFlashOnUpdateEnabled(bool enabled)
{
    if (m_flashOnUpdateEnabled == enabled)
        return;
    m_flashOnUpdateEnabled = enabled;
    emit flashOnUpdateEnabledChanged();
}

DisplaySettings::FavoritesViewMode DisplaySettings::favoritesViewMode() const
{
    return m_favoritesViewMode;
}

void DisplaySettings::setFavoritesViewMode(FavoritesViewMode mode)
{
    if (m_favoritesViewMode == mode)
        return;
    m_favoritesViewMode = mode;
    emit favoritesViewModeChanged();
}

int DisplaySettings::toDisplayAddress(int registerType, int pduAddress) const
{
    return Core::displayAddress(Core::RegisterType(registerType), pduAddress, toCoreConvention(m_addressConvention));
}

int DisplaySettings::toPduAddress(int registerType, int displayAddress) const
{
    return Core::pduAddress(Core::RegisterType(registerType), displayAddress, toCoreConvention(m_addressConvention));
}

} // namespace ModbusViewer::AppLib
