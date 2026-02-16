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
        if (!enabled || !trayIcon || percentage > threshold || percentage <= 0 || charging)
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

        if (percentage > threshold + 5)
        {
            notificationShown = false;
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

    void reset()
    {
        notificationShown = false;
    }

private:
    TrayIcon *trayIcon = nullptr;
    int threshold = 20;
    bool enabled = true;
    bool notificationShown = false;
};
