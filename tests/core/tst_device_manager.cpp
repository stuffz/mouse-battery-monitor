#include "core/device_manager.hpp"
#include "core/fake_mouse.hpp"

#include <QObject>
#include <QTest>

#include <string>

namespace
{

// Lower is preferred, matching the real devices: a cabled mouse reports its
// own battery and charge state, a dongle only relays.
constexpr int CABLE_PRIORITY = 1;
constexpr int DONGLE_PRIORITY = 3;

class TestDeviceManager : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void prefersTheCableWhenBothArePlugged();
    void fallsBackToTheDongle();
    void findsNothingWithNothingPlugged();
    void switchesToTheCableOnceItIsPlugged();
    void staysOnTheCableWhenTheDongleIsThere();
};

void TestDeviceManager::prefersTheCableWhenBothArePlugged()
{
    FakeMice mice;
    // Added worst first, so only the priority sort can put the cable first.
    mice.add("Dongle", DONGLE_PRIORITY);
    mice.add("Cable", CABLE_PRIORITY);
    DeviceManager devices = mice.release();

    QVERIFY(devices.FindAndConnect());

    QCOMPARE(devices.GetDeviceName(), std::wstring(L"Cable"));
}

void TestDeviceManager::fallsBackToTheDongle()
{
    FakeMice mice;
    mice.add("Cable", CABLE_PRIORITY).presence = Presence::Unplugged;
    mice.add("Dongle", DONGLE_PRIORITY);
    DeviceManager devices = mice.release();

    QVERIFY(devices.FindAndConnect());

    QCOMPARE(devices.GetDeviceName(), std::wstring(L"Dongle"));
}

void TestDeviceManager::findsNothingWithNothingPlugged()
{
    FakeMice mice;
    mice.add("Cable", CABLE_PRIORITY).presence = Presence::Unplugged;
    mice.add("Dongle", DONGLE_PRIORITY).presence = Presence::Unplugged;
    DeviceManager devices = mice.release();

    QVERIFY(!devices.FindAndConnect());

    QVERIFY(!devices.IsConnected());
    QCOMPARE(devices.ReadBattery().percentage, -1);
}

void TestDeviceManager::switchesToTheCableOnceItIsPlugged()
{
    FakeMice mice;
    FakeMouse &cable = mice.add("Cable", CABLE_PRIORITY);
    FakeMouse &dongle = mice.add("Dongle", DONGLE_PRIORITY);
    cable.presence = Presence::Unplugged;
    DeviceManager devices = mice.release();
    QVERIFY(devices.FindAndConnect());

    cable.presence = Presence::Plugged;

    QVERIFY(devices.ShouldSwitchDevice());
    QCOMPARE(devices.GetDeviceName(), std::wstring(L"Cable"));
    QVERIFY(!dongle.IsConnected());
}

void TestDeviceManager::staysOnTheCableWhenTheDongleIsThere()
{
    FakeMice mice;
    mice.add("Cable", CABLE_PRIORITY);
    mice.add("Dongle", DONGLE_PRIORITY);
    DeviceManager devices = mice.release();
    QVERIFY(devices.FindAndConnect());

    QVERIFY(!devices.ShouldSwitchDevice());
    QCOMPARE(devices.GetDeviceName(), std::wstring(L"Cable"));
}

} // namespace

QTEST_APPLESS_MAIN(TestDeviceManager)

#include "tst_device_manager.moc"
