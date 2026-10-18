#include "core/battery_monitor.hpp"
#include "core/fake_mouse.hpp"
#include "core/hotplug_scheduler.hpp"

#include <QObject>
#include <QTest>

namespace
{

using Event = HotplugScheduler::Event;

constexpr int NOT_STARTED = -1;
constexpr int ALWAYS_FAILS = 1000;

// Stands in for the platform timers: the test fires them by hand.
class Harness
{
public:
    FakeMice mice;
    FakeMouse &mouse = mice.add("Mouse", 1);
    BatteryMonitor monitor{mice.release()};
    HotplugScheduler hotplug;

    int debounceMs = NOT_STARTED;
    bool retrying = false;

    Harness()
    {
        hotplug.init(&monitor, {[this](int delayMs) { debounceMs = delayMs; },
                                [this] { retrying = true; }, [this] { retrying = false; }});
    }

    void deliver(Event event)
    {
        hotplug.postEvent(event);
        fireDebounce();
    }

    void fireDebounce()
    {
        QVERIFY(debounceMs != NOT_STARTED);
        debounceMs = NOT_STARTED;
        hotplug.onDebounceElapsed();
    }
};

class TestHotplugScheduler : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void ignoresAnEmptyEvent();
    void waitsLongerAfterAnArrivalThanARemoval();
    void handlesOnlyTheLastEventOfABurst();
    void retriesAnArrivalUntilTheMouseAnswers();
    void givesUpAfterTheLastRetry();
    void stopsRetryingWhenTheDeviceGoesAway();
};

void TestHotplugScheduler::ignoresAnEmptyEvent()
{
    Harness h;

    h.hotplug.postEvent(Event::None);

    QCOMPARE(h.debounceMs, NOT_STARTED);
}

// A new hidraw node exists before udev has applied the uaccess rule to it; a
// removal has nothing to wait for.
void TestHotplugScheduler::waitsLongerAfterAnArrivalThanARemoval()
{
    Harness h;

    h.hotplug.postEvent(Event::Arrival);
    const int arrivalMs = h.debounceMs;
    h.hotplug.postEvent(Event::Removal);
    const int removalMs = h.debounceMs;

    QVERIFY(arrivalMs > removalMs);
}

// One replug raises an event per hidraw node, and the dongle has two.
void TestHotplugScheduler::handlesOnlyTheLastEventOfABurst()
{
    Harness h;
    h.monitor.update();

    h.hotplug.postEvent(Event::Removal);
    h.hotplug.postEvent(Event::Arrival);
    h.fireDebounce();

    QVERIFY(h.monitor.hasValidStatus());
}

void TestHotplugScheduler::retriesAnArrivalUntilTheMouseAnswers()
{
    Harness h;
    h.mouse.failingReads = 2;

    h.deliver(Event::Arrival);
    QVERIFY(h.retrying);

    h.hotplug.onRetryElapsed();
    QVERIFY(h.retrying);
    QVERIFY(!h.monitor.hasValidStatus());

    h.hotplug.onRetryElapsed();
    QVERIFY(!h.retrying);
    QVERIFY(h.monitor.hasValidStatus());
}

void TestHotplugScheduler::givesUpAfterTheLastRetry()
{
    Harness h;
    h.mouse.failingReads = ALWAYS_FAILS;
    h.deliver(Event::Arrival);

    for (int retry = 1; retry < HotplugScheduler::MAX_ARRIVAL_RETRIES; ++retry)
    {
        h.hotplug.onRetryElapsed();
        QVERIFY(h.retrying);
    }
    h.hotplug.onRetryElapsed();

    QVERIFY(!h.retrying);
}

void TestHotplugScheduler::stopsRetryingWhenTheDeviceGoesAway()
{
    Harness h;
    h.mouse.failingReads = ALWAYS_FAILS;
    h.deliver(Event::Arrival);
    QVERIFY(h.retrying);

    h.mouse.presence = Presence::Unplugged;
    h.deliver(Event::Removal);

    QVERIFY(!h.retrying);
}

} // namespace

QTEST_APPLESS_MAIN(TestHotplugScheduler)

#include "tst_hotplug_scheduler.moc"
