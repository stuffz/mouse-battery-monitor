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
                handleDisconnected();
            }
            else
            {
                handleConnected(status);
            }
        }
        catch (const std::exception &ex)
        {
            LOG_ERROR("Exception in BatteryMonitor::update: " + string(ex.what()));
            deviceManager.Disconnect();
        }
        catch (...)
        {
            LOG_ERROR("Unknown exception in BatteryMonitor::update");
            deviceManager.Disconnect();
        }
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

    void handleDisconnected()
    {
        LOG_DEBUG("No device connected or read error - attempting reconnection");
        deviceManager.Disconnect();

        if (deviceManager.FindAndConnect())
        {
            LOG_INFO("Device reconnected after mode switch");
        }

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

        if (trayIcon && iconLoader)
        {
            trayIcon->update(
                iconLoader->GetBatteryIcon(status.percentage, status.isCharging),
                buildTooltip(status));
        }

        if (notificationMgr)
        {
            notificationMgr->checkLowBattery(status.percentage, status.isCharging,
                                             deviceManager.GetDeviceName());
        }
    }

    wstring buildTooltip(const DeviceManager::BatteryStatus &status)
    {
        wstringstream ss;
        ss << deviceManager.GetDeviceName() << L"\n"
           << deviceManager.GetConnectionMode() << L"\n"
           << L"Battery: " << status.percentage << L"%";
        return ss.str();
    }
};
