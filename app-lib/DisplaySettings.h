#pragma once

#include <QObject>
#include <QQmlEngine>

namespace ModbusViewer::AppLib {

// One global setting, shared by every view (Normal and Favorites) and every
// address-entry field, per docs/protocol.md's "Addressing convention" section: the
// wire/PDU address never changes, only how it's rendered/entered. The Modicon
// prefix (0x/1x/3x/4x) depends on which register type is being addressed, so
// callers pass Core::RegisterType's raw int alongside the address.
class DisplaySettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(AddressConvention addressConvention READ addressConvention WRITE setAddressConvention NOTIFY
                   addressConventionChanged)
    Q_PROPERTY(bool flashOnUpdateEnabled READ flashOnUpdateEnabled WRITE setFlashOnUpdateEnabled NOTIFY
                   flashOnUpdateEnabledChanged)
    Q_PROPERTY(FavoritesViewMode favoritesViewMode READ favoritesViewMode WRITE setFavoritesViewMode NOTIFY
                   favoritesViewModeChanged)

public:
    enum class AddressConvention { Pdu, Modicon };
    Q_ENUM(AddressConvention)

    // List is the default -- the card view is an opt-in alternative, not a
    // replacement for the existing dense row list.
    enum class FavoritesViewMode { List, Cards };
    Q_ENUM(FavoritesViewMode)

    explicit DisplaySettings(QObject *parent = nullptr);

    AddressConvention addressConvention() const;
    void setAddressConvention(AddressConvention convention);

    bool flashOnUpdateEnabled() const;
    void setFlashOnUpdateEnabled(bool enabled);

    FavoritesViewMode favoritesViewMode() const;
    void setFavoritesViewMode(FavoritesViewMode mode);

    Q_INVOKABLE int toDisplayAddress(int registerType, int pduAddress) const;
    Q_INVOKABLE int toPduAddress(int registerType, int displayAddress) const;

signals:
    void addressConventionChanged();
    void flashOnUpdateEnabledChanged();
    void favoritesViewModeChanged();

private:
    AddressConvention m_addressConvention = AddressConvention::Pdu;
    bool m_flashOnUpdateEnabled = true;
    FavoritesViewMode m_favoritesViewMode = FavoritesViewMode::List;
};

} // namespace ModbusViewer::AppLib
