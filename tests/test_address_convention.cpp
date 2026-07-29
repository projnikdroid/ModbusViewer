#include <QTest>

#include "format/AddressConvention.h"

using namespace ModbusViewer::Core;

class AddressConventionTest : public QObject
{
    Q_OBJECT

private slots:
    void pduConventionIsPassthrough();
    void modiconAddsPerTypeBase();
    void pduAddressIsInverseOfDisplayAddress_data();
    void pduAddressIsInverseOfDisplayAddress();
};

void AddressConventionTest::pduConventionIsPassthrough()
{
    QCOMPARE(displayAddress(RegisterType::HoldingRegister, 0, AddressConvention::Pdu), 0);
    QCOMPARE(displayAddress(RegisterType::HoldingRegister, 41, AddressConvention::Pdu), 41);
    QCOMPARE(pduAddress(RegisterType::Coil, 100, AddressConvention::Pdu), 100);
}

// Standard Modicon 5-digit-per-region bases: 0xxxx coils, 1xxxx discrete inputs,
// 3xxxx input registers, 4xxxx holding registers -- each region's "1" is address 0.
void AddressConventionTest::modiconAddsPerTypeBase()
{
    QCOMPARE(displayAddress(RegisterType::Coil, 0, AddressConvention::Modicon), 1);
    QCOMPARE(displayAddress(RegisterType::DiscreteInput, 0, AddressConvention::Modicon), 10001);
    QCOMPARE(displayAddress(RegisterType::InputRegister, 0, AddressConvention::Modicon), 30001);
    QCOMPARE(displayAddress(RegisterType::HoldingRegister, 0, AddressConvention::Modicon), 40001);

    QCOMPARE(displayAddress(RegisterType::HoldingRegister, 41, AddressConvention::Modicon), 40042);
}

void AddressConventionTest::pduAddressIsInverseOfDisplayAddress_data()
{
    QTest::addColumn<int>("registerType");
    QTest::addColumn<int>("convention");
    QTest::addColumn<int>("pdu");

    QTest::newRow("Pdu holding") << int(RegisterType::HoldingRegister) << int(AddressConvention::Pdu) << 41;
    QTest::newRow("Modicon holding") << int(RegisterType::HoldingRegister) << int(AddressConvention::Modicon) << 41;
    QTest::newRow("Modicon coil") << int(RegisterType::Coil) << int(AddressConvention::Modicon) << 0;
    QTest::newRow("Modicon discrete input") << int(RegisterType::DiscreteInput) << int(AddressConvention::Modicon) << 99;
    QTest::newRow("Modicon input register") << int(RegisterType::InputRegister) << int(AddressConvention::Modicon) << 1234;
}

void AddressConventionTest::pduAddressIsInverseOfDisplayAddress()
{
    QFETCH(int, registerType);
    QFETCH(int, convention);
    QFETCH(int, pdu);

    const auto type = RegisterType(registerType);
    const auto conv = AddressConvention(convention);

    const int display = displayAddress(type, pdu, conv);
    QCOMPARE(pduAddress(type, display, conv), pdu);
}

QTEST_APPLESS_MAIN(AddressConventionTest)
#include "test_address_convention.moc"
