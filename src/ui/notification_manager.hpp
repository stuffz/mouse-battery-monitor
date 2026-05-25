#pragma once

#include <string>
#include <sstream>
#include "tray_icon.hpp"
#include "core/logger.hpp"

using std::wstring;
using std::wstringstream;

class NotificationManager
{
public:
    NotificationManager() = default;

    void setTrayIcon(TrayIcon *icon)
    {
        trayIcon = icon;
    }

    void setThreshold(int value)
    {
        threshold = value;
    }

    void setEnabled(bool value)
    {
        enabled = value;
    }

    void checkLowBattery(int percentage, bool charging, const wstring &deviceName)
    {
        if (!enabled || !trayIcon || percentage <= 0)
        {
            return;
        }

        // Reset notification flag when battery recovers above threshold + hysteresis
        if (percentage > threshold + 5)
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
            wstringstream title;
            title << deviceName << L" - Low Battery";

            wstringstream msg;
            msg << L"Battery at " << percentage << L"%";

            trayIcon->showNotification(title.str(), msg.str());
            notificationShown = true;
            LOG_INFO("Low battery notification shown");
        }
    }

    void triggerTestNotification(int percentage, const wstring &deviceName)
    {
        if (!trayIcon)
        {
            return;
        }

        LOG_INFO("Triggering test low battery notification");

        wstringstream title;
        if (!deviceName.empty())
        {
            title << deviceName << L" - Low Battery";
        }
        else
        {
            title << L"Mouse - Low Battery";
        }

        wstringstream msg;
        msg << L"Battery at " << percentage << L"%";

        trayIcon->showNotification(title.str(), msg.str());
    }

private:
    TrayIcon *trayIcon = nullptr;
    int threshold = 20;
    bool enabled = true;
    bool notificationShown = false;
};
