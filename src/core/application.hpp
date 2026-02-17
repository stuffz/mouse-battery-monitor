#pragma once

#include <windows.h>
#include <shobjidl.h>
#include <dbt.h>
#include <filesystem>
#include "config.hpp"
#include "logger.hpp"
#include "battery_monitor.hpp"
#include "ui/icon_loader.hpp"
#include "ui/tray_icon.hpp"
#include "ui/notification_manager.hpp"
#include "ui/app_window.hpp"
#include "ui/context_menu.hpp"

namespace fs = std::filesystem;

class Application
{
public:
    struct Constants
    {
        static constexpr UINT WM_TRAYICON = WM_USER + 1;
        static constexpr UINT ID_TRAY_ICON = 1;
        static constexpr UINT ID_TIMER_UPDATE = 1;
        static constexpr UINT ID_TIMER_DEVICE_CHANGE = 2;
        static constexpr UINT ID_TIMER_ARRIVAL_RETRY = 3;
        static constexpr int ARRIVAL_DEBOUNCE_MS = 1500;
        static constexpr int ARRIVAL_RETRY_MS = 3000;
        static constexpr int MAX_ARRIVAL_RETRIES = 3;
        static constexpr UINT ID_MENU_UPDATE = 1001;
        static constexpr UINT ID_MENU_TRIGGER_LOW_BATTERY = 1002;
        static constexpr UINT ID_MENU_ABOUT = 1003;
        static constexpr UINT ID_MENU_EXIT = 1004;
        static constexpr wchar_t WINDOW_CLASS[] = L"MouseBatteryMonitorClass";
        static constexpr wchar_t WINDOW_TITLE[] = L"Mouse Battery Monitor";
    };

    static Application &instance()
    {
        static Application app;
        return app;
    }

    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;

    void setBuildInfo(const char *date, const char *hash)
    {
        window.setBuildInfo(date, hash);
    }

    int run(HINSTANCE hInstance, WNDPROC wndProc)
    {
        this->hInstance = hInstance;

        if (!initialize(wndProc))
            return 1;

        int result = messageLoop();
        shutdown();
        return result;
    }

    // Event handlers called from WndProc
    void onTrayIconClick()
    {
        showContextMenu();
    }

    void onMenuCommand(UINT commandId)
    {
        switch (commandId)
        {
        case Constants::ID_MENU_UPDATE:
            batteryMonitor.update();
            break;
        case Constants::ID_MENU_TRIGGER_LOW_BATTERY:
            batteryMonitor.triggerTestNotification(config.GetLowBatteryThreshold());
            break;
        case Constants::ID_MENU_ABOUT:
            window.showAboutDialog();
            break;
        case Constants::ID_MENU_EXIT:
            PostQuitMessage(0);
            break;
        }
    }

    void onTimer(UINT_PTR timerId)
    {
        if (timerId == Constants::ID_TIMER_UPDATE)
        {
            batteryMonitor.update();
        }
        else if (timerId == Constants::ID_TIMER_DEVICE_CHANGE)
        {
            window.killTimer(Constants::ID_TIMER_DEVICE_CHANGE);

            if (pendingDeviceEvent == DBT_DEVICEREMOVECOMPLETE)
            {
                LOG_DEBUG("Device change timer fired - USB REMOVAL event");
                arrivalRetryCount = 0;
                window.killTimer(Constants::ID_TIMER_ARRIVAL_RETRY);
                batteryMonitor.onDeviceRemoved();
            }
            else if (pendingDeviceEvent == DBT_DEVICEARRIVAL)
            {
                LOG_DEBUG("Device change timer fired - USB ARRIVAL event");
                arrivalRetryCount = 0;
                batteryMonitor.onDeviceArrived();

                if (!batteryMonitor.hasValidStatus())
                {
                    LOG_DEBUG("First arrival read failed - scheduling retry in " +
                              std::to_string(Constants::ARRIVAL_RETRY_MS) + "ms");
                    window.setDeviceChangeTimer(Constants::ID_TIMER_ARRIVAL_RETRY,
                                                Constants::ARRIVAL_RETRY_MS);
                }
            }
            else
            {
                LOG_DEBUG("Device change timer fired - generic update");
                batteryMonitor.update();
            }

            pendingDeviceEvent = 0;
        }
        else if (timerId == Constants::ID_TIMER_ARRIVAL_RETRY)
        {
            arrivalRetryCount++;
            LOG_DEBUG("Arrival retry " + std::to_string(arrivalRetryCount) +
                      "/" + std::to_string(Constants::MAX_ARRIVAL_RETRIES));

            batteryMonitor.update();

            if (batteryMonitor.hasValidStatus() ||
                arrivalRetryCount >= Constants::MAX_ARRIVAL_RETRIES)
            {
                window.killTimer(Constants::ID_TIMER_ARRIVAL_RETRY);
                if (batteryMonitor.hasValidStatus())
                {
                    LOG_DEBUG("Arrival retry succeeded - battery status acquired");
                }
                else
                {
                    LOG_DEBUG("Arrival retries exhausted - giving up");
                }
                arrivalRetryCount = 0;
            }
        }
    }

    void onDeviceChange(WPARAM wParam, LPARAM lParam)
    {
        if (wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVICEREMOVECOMPLETE)
        {
            PDEV_BROADCAST_HDR hdr = reinterpret_cast<PDEV_BROADCAST_HDR>(lParam);
            if (hdr && hdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
            {
                pendingDeviceEvent = wParam;
                LOG_DEBUG(string("USB event received: ") +
                          (wParam == DBT_DEVICEARRIVAL ? "DEVICE_ARRIVAL" : "DEVICE_REMOVE_COMPLETE"));

                int debounceMs = (wParam == DBT_DEVICEARRIVAL)
                                     ? Constants::ARRIVAL_DEBOUNCE_MS
                                     : 100;
                window.setDeviceChangeTimer(Constants::ID_TIMER_DEVICE_CHANGE, debounceMs);
            }
        }
    }

    void onDestroy()
    {
        window.destroy();
        PostQuitMessage(0);
    }

    void onTaskbarCreated()
    {
        trayIcon.reAdd();
    }

    // Accessors
    Config &getConfig() { return config; }
    AppWindow &getWindow() { return window; }
    UINT getTaskbarCreatedMsg() const { return taskbarCreatedMsg; }

private:
    Application() = default;

    HINSTANCE hInstance = nullptr;
    Config config;
    IconLoader iconLoader;
    TrayIcon trayIcon;
    NotificationManager notificationManager;
    AppWindow window;
    BatteryMonitor batteryMonitor;
    UINT taskbarCreatedMsg = 0;
    WPARAM pendingDeviceEvent = 0;
    int arrivalRetryCount = 0;

    bool initialize(WNDPROC wndProc)
    {
        setAppUserModelID();
        LOG_DEBUG("Entered Application::initialize");

        if (!loadConfig())
            return false;

        LOG_INFO("Starting Mouse Battery Monitor");

        if (!loadResources())
            return false;

        if (!createWindow(wndProc))
            return false;

        setupComponents();
        return true;
    }

    void setAppUserModelID()
    {
        SetCurrentProcessExplicitAppUserModelID(L"Mouse Battery Monitor");
    }

    bool loadConfig()
    {
        if (!config.Load("config.ini"))
        {
            LOG_ERROR("Failed to load config.ini, using defaults");
        }
        else
        {
            LOG_DEBUG("Loaded configuration from config.ini");
        }

        Logger::Instance().SetDebugMode(config.GetDebugMode());
        Logger::Instance().SetLogFile("battery_monitor.log");
        LOG_DEBUG("Logger configured");

        return true;
    }

    bool loadResources()
    {
        fs::path resourceDir = getResourceDirectory();

        if (!iconLoader.LoadIcons(resourceDir.wstring()))
        {
            LOG_ERROR("Failed to load icons from resources folder");
            MessageBoxW(nullptr,
                        L"Failed to load icon resources. Make sure the 'resources' folder exists with required PNG files.",
                        L"Error", MB_OK | MB_ICONERROR);
            return false;
        }

        LOG_DEBUG("Icon load succeeded");
        return true;
    }

    fs::path getResourceDirectory()
    {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        fs::path exeDir = fs::path(exePath).parent_path();
        fs::path resourceDir = exeDir / L"resources";

        LOG_DEBUG("Executable directory: " + exeDir.u8string());
        LOG_DEBUG("Resource directory: " + resourceDir.u8string());

        return resourceDir;
    }

    bool createWindow(WNDPROC wndProc)
    {
        window.init(&config);

        if (!window.create(hInstance, wndProc, Constants::WINDOW_CLASS, Constants::WINDOW_TITLE))
        {
            MessageBoxW(nullptr, L"Failed to create window", L"Error", MB_OK | MB_ICONERROR);
            return false;
        }
        return true;
    }

    void setupComponents()
    {
        trayIcon.init(window.handle(), Constants::WM_TRAYICON, Constants::ID_TRAY_ICON,
                      iconLoader.GetDisconnectedIcon(),
                      L"Mouse Battery Monitor\nNo device connected");

        notificationManager.setTrayIcon(&trayIcon);
        notificationManager.setThreshold(config.GetLowBatteryThreshold());
        notificationManager.setEnabled(config.GetShowNotifications());

        batteryMonitor.init(&trayIcon, &iconLoader, &notificationManager);

        taskbarCreatedMsg = window.registerTaskbarCreatedMessage();
        window.registerDeviceNotifications();
        window.setUpdateTimer(Constants::ID_TIMER_UPDATE, config.GetUpdateIntervalSeconds());

        batteryMonitor.update();
    }

    void showContextMenu()
    {
        ContextMenu menu;
        menu.addItem(Constants::ID_MENU_UPDATE, L"Update Now");
        menu.addItem(Constants::ID_MENU_TRIGGER_LOW_BATTERY, L"Trigger Low Battery", config.GetDebugMode());
        menu.addSeparator();
        menu.addItem(Constants::ID_MENU_ABOUT, L"About");
        menu.addItem(Constants::ID_MENU_EXIT, L"Exit");
        menu.show(window.handle());
    }

    int messageLoop()
    {
        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        return static_cast<int>(msg.wParam);
    }

    void shutdown()
    {
        trayIcon.remove();
        batteryMonitor.devices().Disconnect();
        LOG_INFO("Shutting down");
    }
};
