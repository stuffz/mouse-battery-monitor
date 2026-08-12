#pragma once

#include "core/logger.hpp"
#include "tray_icon.hpp"
#include <sstream>
#include <string>

class NotificationManager
{
public:
    NotificationManager() = default;

    void setTrayIcon(TrayIcon *icon) { trayIcon = icon; }

    void setThreshold(int value) { threshold = value; }

    void setEnabled(bool value) { enabled = value; }

    void checkLowBattery(int percentage, bool charging, const std::wstring &deviceName)
    {
        if (!enabled || !trayIcon || percentage <= 0)
        {
            return;
        }

        if (percentage > threshold + RECOVERY_HYSTERESIS)
        {
            notificationShown = false;
            return;
        }

        if (percentage > threshold || charging)
        {
            return;
        }

        if (!notificationShown)
        {
            std::wstringstream title;
            title << deviceName << L" - Low Battery";

            std::wstringstream msg;
            msg << L"Battery at " << percentage << L"%";

            trayIcon->showNotification(title.str(), msg.str());
            notificationShown = true;
            LOG_INFO("Low battery notification shown");
        }
    }

    void triggerTestNotification(int percentage, const std::wstring &deviceName)
    {
        if (!trayIcon)
        {
            return;
        }

        LOG_INFO("Triggering test low battery notification");

        std::wstringstream title;
        if (!deviceName.empty())
        {
            title << deviceName << L" - Low Battery";
        }
        else
        {
            title << L"Mouse - Low Battery";
        }

        std::wstringstream msg;
        msg << L"Battery at " << percentage << L"%";

        trayIcon->showNotification(title.str(), msg.str());
    }

private:
    static constexpr int RECOVERY_HYSTERESIS = 5;

    TrayIcon *trayIcon = nullptr;
    int threshold = 20;
    bool enabled = true;
    bool notificationShown = false;
};
