#pragma once

#include <string>
#include <sstream>
#include "device_manager.hpp"
#include "logger.hpp"
#include "ui/icon_loader.hpp"
#include "ui/tray_icon.hpp"
#include "ui/notification_manager.hpp"

using std::string;
using std::wstring;
using std::wstringstream;

class BatteryMonitor
{
public:
    BatteryMonitor() = default;

    void init(TrayIcon *tray, IconLoader *icons, NotificationManager *notifications)
    {
        trayIcon = tray;
        iconLoader = icons;
        notificationMgr = notifications;
    }

    void update()
    {
        LOG_DEBUG("Updating battery status");

        ensureConnected();

        try
        {
            auto status = deviceManager.ReadBattery();

            if (status.percentage < 0)
            {
                handleReadFailure();
            }
            else
            {
                consecutiveFailures = 0;
                handleConnected(status);
            }
        }
        catch (const std::exception &ex)
        {
            LOG_ERROR("Exception in BatteryMonitor::update: " + string(ex.what()));
            handleReadFailure();
        }
        catch (...)
        {
            LOG_ERROR("Unknown exception in BatteryMonitor::update");
            handleReadFailure();
        }
    }

    // Called from Application when a real USB DBT_DEVICEREMOVECOMPLETE event fires
    void onDeviceRemoved()
    {
        LOG_INFO("USB device removal event - forcing disconnect");
        consecutiveFailures = 0;
        lastKnownStatus = {};
        lastKnownDeviceName.clear();
        lastKnownConnectionMode.clear();
        deviceManager.Disconnect();

        if (trayIcon && iconLoader)
        {
            trayIcon->update(iconLoader->GetDisconnectedIcon(),
                             L"Mouse Battery Monitor\nNo device connected");
        }
    }

    bool hasValidStatus() const
    {
        return lastKnownStatus.percentage >= 0;
    }

    // Called from Application when a real USB DBT_DEVICEARRIVAL event fires
    void onDeviceArrived()
    {
        LOG_INFO("USB device arrival event - attempting connection");
        consecutiveFailures = 0;
        update();
    }

    void triggerTestNotification(int fallbackPercentage)
    {
        auto status = deviceManager.ReadBattery();
        int percentage = status.percentage >= 0 ? status.percentage : fallbackPercentage;
        wstring name = deviceManager.IsConnected() ? deviceManager.GetDeviceName() : L"";
        notificationMgr->triggerTestNotification(percentage, name);
    }

    DeviceManager &devices() { return deviceManager; }
    const DeviceManager &devices() const { return deviceManager; }

private:
    DeviceManager deviceManager;
    TrayIcon *trayIcon = nullptr;
    IconLoader *iconLoader = nullptr;
    NotificationManager *notificationMgr = nullptr;

    // Cached last good state for sleep tolerance
    DeviceManager::BatteryStatus lastKnownStatus{};
    wstring lastKnownDeviceName;
    wstring lastKnownConnectionMode;
    int consecutiveFailures = 0;

    void ensureConnected()
    {
        if (!deviceManager.IsConnected())
        {
            LOG_DEBUG("Device not connected, attempting to find and connect");
            if (deviceManager.FindAndConnect())
            {
                LOG_INFO("Device connected successfully");
            }
        }
        else
        {
            deviceManager.ShouldSwitchDevice();
        }
    }

    void handleReadFailure()
    {
        consecutiveFailures++;
        LOG_DEBUG("Battery read failed (consecutive failures: " +
                  std::to_string(consecutiveFailures) + ")");

        // Check if the dongle/device handle is still open
        bool donglePresent = deviceManager.IsConnected();
        bool hasLastKnown = lastKnownStatus.percentage >= 0;

        LOG_DEBUG("Dongle still present: " + string(donglePresent ? "Yes" : "No") +
                  ", Has cached battery: " + string(hasLastKnown ? "Yes" : "No") +
                  (hasLastKnown ? " (" + std::to_string(lastKnownStatus.percentage) + "%)" : ""));

        if (donglePresent && hasLastKnown)
        {
            // Dongle handle is still valid but mouse isn't responding - likely sleeping
            LOG_DEBUG("Mouse appears to be sleeping - keeping last known battery: " +
                      std::to_string(lastKnownStatus.percentage) + "%");
        }
        else
        {
            // No dongle or never had a good read - try reconnection
            LOG_DEBUG("No dongle handle or no cached status - attempting reconnection");
            deviceManager.Disconnect();

            if (deviceManager.FindAndConnect())
            {
                LOG_INFO("Device reconnected after mode switch");
            }
            else
            {
                handleDisconnected();
            }
        }
    }

    void handleDisconnected()
    {
        LOG_DEBUG("Device fully disconnected - showing disconnected icon");
        lastKnownStatus = {};
        lastKnownDeviceName.clear();
        lastKnownConnectionMode.clear();

        if (trayIcon && iconLoader)
        {
            trayIcon->update(iconLoader->GetDisconnectedIcon(),
                             L"Mouse Battery Monitor\nNo device connected");
        }
    }

    void handleConnected(const DeviceManager::BatteryStatus &status)
    {
        LOG_DEBUG("Battery: " + std::to_string(status.percentage) + "%, Charging: " +
                  (status.isCharging ? "Yes" : "No"));

        // Cache the good reading
        lastKnownStatus = status;
        lastKnownDeviceName = deviceManager.GetDeviceName();
        lastKnownConnectionMode = deviceManager.GetConnectionMode();

        updateTray();

        if (notificationMgr)
        {
            notificationMgr->checkLowBattery(status.percentage, status.isCharging,
                                             deviceManager.GetDeviceName());
        }
    }

    void updateTray()
    {
        if (!trayIcon || !iconLoader)
            return;

        if (lastKnownStatus.percentage < 0)
        {
            trayIcon->update(iconLoader->GetDisconnectedIcon(),
                             L"Mouse Battery Monitor\nNo device connected");
            return;
        }

        trayIcon->update(
            iconLoader->GetBatteryIcon(lastKnownStatus.percentage, lastKnownStatus.isCharging),
            buildTooltip());
    }

    wstring buildTooltip()
    {
        wstringstream ss;
        ss << lastKnownDeviceName << L"\n"
           << lastKnownConnectionMode << L"\n"
           << L"Battery: " << lastKnownStatus.percentage << L"%";
        return ss.str();
    }
};
