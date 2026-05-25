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

    // Main polling loop entry point.
    void update()
    {
        try
        {
            if (!deviceManager.IsConnected())
            {
                if (deviceManager.FindAndConnect())
                {
                    LOG_INFO("Device connected successfully");
                }
                else
                {
                    handleDisconnected();
                    return;
                }
            }
            else
            {
                deviceManager.ShouldSwitchDevice();
            }

            auto status = deviceManager.ReadBattery();

            if (status.percentage >= 0)
            {
                handleValidRead(status);
            }
            else if (deviceManager.IsConnected())
            {
                // Handle still open, but read failed -> mouse is sleeping.
                // Keep cached status to avoid UI flicker.
                LOG_DEBUG("Read failed - mouse appears to be sleeping");
            }
            else
            {
                handleDisconnected();
            }
        }
        catch (const std::exception &ex)
        {
            LOG_ERROR("Exception during battery update: " + string(ex.what()));
            deviceManager.Disconnect();
            handleDisconnected();
        }
    }

    void onDeviceRemoved()
    {
        LOG_INFO("USB removal detected");
        deviceManager.Disconnect();
        handleDisconnected();
    }

    void onDeviceArrived()
    {
        LOG_INFO("USB arrival detected");
        update();
    }

    bool hasValidStatus() const
    {
        return cachedStatus.percentage >= 0;
    }

    void triggerTestNotification(int fallbackPercentage)
    {
        if (!notificationMgr)
            return;

        auto status = deviceManager.ReadBattery();
        int percentage = status.percentage >= 0 ? status.percentage : fallbackPercentage;
        wstring name = deviceManager.IsConnected() ? deviceManager.GetDeviceName() : L"";
        notificationMgr->triggerTestNotification(percentage, name);
    }

    DeviceManager &devices() { return deviceManager; }

private:
    DeviceManager deviceManager;
    TrayIcon *trayIcon = nullptr;
    IconLoader *iconLoader = nullptr;
    NotificationManager *notificationMgr = nullptr;

    DeviceManager::BatteryStatus cachedStatus{};
    wstring cachedDeviceName;
    wstring cachedConnectionMode;

    void handleValidRead(const DeviceManager::BatteryStatus &status)
    {
        LOG_DEBUG("Battery: " + std::to_string(status.percentage) + "%, Charging: " +
                  (status.isCharging ? "Yes" : "No"));

        cachedStatus = status;
        cachedDeviceName = deviceManager.GetDeviceName();
        cachedConnectionMode = deviceManager.GetConnectionMode();

        updateTray();

        if (notificationMgr)
        {
            notificationMgr->checkLowBattery(status.percentage,
                                             status.isCharging,
                                             cachedDeviceName);
        }
    }

    void handleDisconnected()
    {
        cachedStatus = {};
        cachedDeviceName.clear();
        cachedConnectionMode.clear();

        if (trayIcon && iconLoader)
        {
            trayIcon->update(iconLoader->GetDisconnectedIcon(),
                             L"Mouse Battery Monitor\nNo device connected");
        }
    }

    void updateTray()
    {
        if (!trayIcon || !iconLoader || cachedStatus.percentage < 0)
            return;

        trayIcon->update(
            iconLoader->GetBatteryIcon(cachedStatus.percentage, cachedStatus.isCharging),
            buildTooltip());
    }

    wstring buildTooltip() const
    {
        wstringstream ss;
        ss << cachedDeviceName << L"\n"
           << cachedConnectionMode << L"\n"
           << L"Battery: " << cachedStatus.percentage << L"%";
        return ss.str();
    }
};
