#pragma once

#include <QObject>
#include <QQmlEngine>

namespace ModbusViewer::AppLib {

// One global setting, shared by every view (Normal now, Favorites later) and every
// address-entry field, per docs/protocol.md's "Addressing convention" section: the
// wire/PDU address never changes, only how it's rendered/entered. Scoped to
// HoldingRegister for now since that's the only register type any view actually
// uses (RegisterTableModel); revisit if Coil/DiscreteInput/InputRegister views land.
class DisplaySettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(AddressConvention addressConvention READ addressConvention WRITE setAddressConvention NOTIFY
                   addressConventionChanged)
    Q_PROPERTY(bool flashOnUpdateEnabled READ flashOnUpdateEnabled WRITE setFlashOnUpdateEnabled NOTIFY
                   flashOnUpdateEnabledChanged)

public:
    enum class AddressConvention { Pdu, Modicon };
    Q_ENUM(AddressConvention)

    explicit DisplaySettings(QObject *parent = nullptr);

    AddressConvention addressConvention() const;
    void setAddressConvention(AddressConvention convention);

    bool flashOnUpdateEnabled() const;
    void setFlashOnUpdateEnabled(bool enabled);

    Q_INVOKABLE int toDisplayAddress(int pduAddress) const;
    Q_INVOKABLE int toPduAddress(int displayAddress) const;

signals:
    void addressConventionChanged();
    void flashOnUpdateEnabledChanged();

private:
    AddressConvention m_addressConvention = AddressConvention::Pdu;
    bool m_flashOnUpdateEnabled = true;
};

} // namespace ModbusViewer::AppLib
