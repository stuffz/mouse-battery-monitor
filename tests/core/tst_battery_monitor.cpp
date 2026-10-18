#include "core/battery_monitor.hpp"
#include "core/fake_mouse.hpp"

#include <QObject>
#include <QTest>

namespace
{

class TestBatteryMonitor : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void showsNoReadingWithNothingPlugged();
    void readsTheMouseOnTheFirstUpdate();
    void keepsTheLastReadingWhileTheMouseSleeps();
    void keepsTheMouseWhenAnotherDeviceIsRemoved();
    void clearsTheReadingWhenTheMouseIsRemoved();
    void readsTheMouseAgainOnceItIsBack();
};

void TestBatteryMonitor::showsNoReadingWithNothingPlugged()
{
    FakeMice mice;
    mice.add("Mouse", 1).presence = Presence::Unplugged;
    BatteryMonitor monitor(mice.release());

    monitor.update();

    QVERIFY(!monitor.hasValidStatus());
}

void TestBatteryMonitor::readsTheMouseOnTheFirstUpdate()
{
    FakeMice mice;
    mice.add("Mouse", 1);
    BatteryMonitor monitor(mice.release());

    monitor.update();

    QVERIFY(monitor.hasValidStatus());
}

// A sleeping mouse stops answering but its dongle stays put. Clearing the
// reading would flash "No device connected" every few minutes.
void TestBatteryMonitor::keepsTheLastReadingWhileTheMouseSleeps()
{
    FakeMice mice;
    FakeMouse &mouse = mice.add("Mouse", 1);
    BatteryMonitor monitor(mice.release());
    monitor.update();

    mouse.failingReads = 1;
    monitor.update();

    QVERIFY(monitor.hasValidStatus());
}

// Every hidraw removal reaches the monitor, whichever device it was: the
// monitor's own USB hub drops off whenever the screen sleeps.
void TestBatteryMonitor::keepsTheMouseWhenAnotherDeviceIsRemoved()
{
    FakeMice mice;
    mice.add("Mouse", 1);
    BatteryMonitor monitor(mice.release());
    monitor.update();

    monitor.onDeviceRemoved();

    QVERIFY(monitor.devices().IsConnected());
    QVERIFY(monitor.hasValidStatus());
}

void TestBatteryMonitor::clearsTheReadingWhenTheMouseIsRemoved()
{
    FakeMice mice;
    FakeMouse &mouse = mice.add("Mouse", 1);
    BatteryMonitor monitor(mice.release());
    monitor.update();

    mouse.presence = Presence::Unplugged;
    monitor.onDeviceRemoved();

    QVERIFY(!monitor.hasValidStatus());
}

void TestBatteryMonitor::readsTheMouseAgainOnceItIsBack()
{
    FakeMice mice;
    FakeMouse &mouse = mice.add("Mouse", 1);
    BatteryMonitor monitor(mice.release());
    monitor.update();
    mouse.presence = Presence::Unplugged;
    monitor.onDeviceRemoved();

    mouse.presence = Presence::Plugged;
    monitor.update();

    QVERIFY(monitor.hasValidStatus());
}

} // namespace

QTEST_APPLESS_MAIN(TestBatteryMonitor)

#include "tst_battery_monitor.moc"
