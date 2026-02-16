#pragma once

#include <windows.h>
#include <dbt.h>
#include <hidsdi.h>
#include <sstream>
#include "core/config.hpp"
#include "core/logger.hpp"

using std::wstringstream;

class AppWindow
{
public:
    AppWindow() = default;
    ~AppWindow() = default;

    AppWindow(const AppWindow &) = delete;
    AppWindow &operator=(const AppWindow &) = delete;

    void init(const Config *cfg)
    {
        config = cfg;
    }

    void setBuildInfo(const char *date, const char *hash)
    {
        buildDate = date;
        gitHash = hash;
    }

    bool create(HINSTANCE instance, WNDPROC wndProc, const wchar_t *className, const wchar_t *title)
    {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = wndProc;
        wc.hInstance = instance;
        wc.lpszClassName = className;

        if (RegisterClassExW(&wc) == 0)
        {
            LOG_ERROR("Failed to register window class");
            return false;
        }

        hwnd = CreateWindowExW(
            0,
            className,
            title,
            0,
            0, 0, 0, 0,
            nullptr,
            nullptr,
            instance,
            nullptr);

        if (!hwnd)
        {
            LOG_ERROR("Failed to create window");
            return false;
        }

        LOG_DEBUG("Hidden window created for message loop");
        return true;
    }

    void registerDeviceNotifications()
    {
        if (!hwnd)
        {
            return;
        }

        DEV_BROADCAST_DEVICEINTERFACE_W filter = {};
        filter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE_W);
        filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
        HidD_GetHidGuid(&filter.dbcc_classguid);

        deviceNotify = RegisterDeviceNotificationW(
            hwnd,
            &filter,
            DEVICE_NOTIFY_WINDOW_HANDLE);

        if (deviceNotify)
        {
            LOG_DEBUG("USB device notifications registered");
        }
        else
        {
            LOG_ERROR("Failed to register device notifications");
        }
    }

    UINT registerTaskbarCreatedMessage()
    {
        UINT msg = RegisterWindowMessageW(L"TaskbarCreated");
        if (msg == 0)
        {
            LOG_ERROR("Failed to register TaskbarCreated message");
        }
        else
        {
            LOG_DEBUG("Registered TaskbarCreated message");
        }
        return msg;
    }

    void setUpdateTimer(UINT timerId, int intervalSeconds)
    {
        if (hwnd)
        {
            SetTimer(hwnd, timerId, intervalSeconds * 1000, nullptr);
            LOG_DEBUG("Update timer set for " + std::to_string(intervalSeconds) + " seconds");
        }
    }

    void setDeviceChangeTimer(UINT timerId, int delayMs)
    {
        if (hwnd)
        {
            SetTimer(hwnd, timerId, delayMs, nullptr);
        }
    }

    void killTimer(UINT timerId)
    {
        if (hwnd)
        {
            KillTimer(hwnd, timerId);
        }
    }

    void destroy()
    {
        if (deviceNotify)
        {
            UnregisterDeviceNotification(deviceNotify);
            deviceNotify = nullptr;
        }
    }

    HWND handle() const
    {
        return hwnd;
    }

    void showAboutDialog()
    {
        wstringstream ss;
        ss << L"Mouse Battery Monitor\n\n"
           << L"Build Date: " << buildDate << L"\n"
           << L"Git Hash: " << gitHash << L"\n\n"
           << L"Monitors battery status for Endgame Gear and VAXEE wireless mice.\n\n";

        if (config)
        {
            ss << L"Update Interval: " << config->GetUpdateIntervalSeconds() << L" seconds\n"
               << L"Low Battery Threshold: " << config->GetLowBatteryThreshold() << L"%\n"
               << L"Debug Mode: " << (config->GetDebugMode() ? L"Enabled" : L"Disabled");
        }

        MessageBoxW(hwnd, ss.str().c_str(),
                    L"About Mouse Battery Monitor",
                    MB_OK | MB_ICONINFORMATION);
    }

private:
    HWND hwnd = nullptr;
    HDEVNOTIFY deviceNotify = nullptr;
    const Config *config = nullptr;
    const char *buildDate = "unknown";
    const char *gitHash = "unknown";
};
