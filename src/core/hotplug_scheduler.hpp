#pragma once

#include "core/battery_monitor.hpp"
#include "core/logger.hpp"

#include <functional>
#include <string>
#include <utility>

class HotplugScheduler
{
public:
    enum class Event
    {
        None,
        Arrival,
        Removal
    };

    static constexpr int ARRIVAL_DEBOUNCE_MS = 1500;
    static constexpr int REMOVAL_DEBOUNCE_MS = 100;
    static constexpr int ARRIVAL_RETRY_MS = 3000;
    static constexpr int MAX_ARRIVAL_RETRIES = 3;

    // startRetry must schedule a *repeating* timer at ARRIVAL_RETRY_MS.
    struct Timers
    {
        std::function<void(int delayMs)> startDebounce;
        std::function<void()> startRetry;
        std::function<void()> stopRetry;
    };

    void init(BatteryMonitor *monitor, Timers platformTimers)
    {
        batteryMonitor = monitor;
        timers = std::move(platformTimers);
    }

    void postEvent(Event event)
    {
        if (event == Event::None)
        {
            return;
        }

        pending = event;

        LOG_DEBUG(std::string("USB event received: ") +
                  (event == Event::Arrival ? "DEVICE_ARRIVAL" : "DEVICE_REMOVE_COMPLETE"));

        timers.startDebounce(event == Event::Arrival ? ARRIVAL_DEBOUNCE_MS : REMOVAL_DEBOUNCE_MS);
    }

    void onDebounceElapsed()
    {
        if (pending == Event::Removal)
        {
            LOG_DEBUG("Device change timer fired - USB REMOVAL event");
            retryCount = 0;
            timers.stopRetry();
            batteryMonitor->onDeviceRemoved();
        }
        else if (pending == Event::Arrival)
        {
            LOG_DEBUG("Device change timer fired - USB ARRIVAL event");
            retryCount = 0;
            batteryMonitor->onDeviceArrived();

            if (!batteryMonitor->hasValidStatus())
            {
                LOG_DEBUG("First arrival read failed - scheduling retry in " +
                          std::to_string(ARRIVAL_RETRY_MS) + "ms");
                timers.startRetry();
            }
        }
        else
        {
            LOG_DEBUG("Device change timer fired - generic update");
            batteryMonitor->update();
        }

        pending = Event::None;
    }

    void onRetryElapsed()
    {
        retryCount++;
        LOG_DEBUG("Arrival retry " + std::to_string(retryCount) + "/" +
                  std::to_string(MAX_ARRIVAL_RETRIES));

        batteryMonitor->update();

        if (batteryMonitor->hasValidStatus() || retryCount >= MAX_ARRIVAL_RETRIES)
        {
            timers.stopRetry();

            if (batteryMonitor->hasValidStatus())
            {
                LOG_DEBUG("Arrival retry succeeded - battery status acquired");
            }
            else
            {
                LOG_DEBUG("Arrival retries exhausted - giving up");
            }

            retryCount = 0;
        }
    }

private:
    BatteryMonitor *batteryMonitor = nullptr;
    Timers timers;
    Event pending = Event::None;
    int retryCount = 0;
};
