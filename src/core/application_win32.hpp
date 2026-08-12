#pragma once

#include <windows.h>

#include <dbt.h>
#include <shobjidl.h>

#include <filesystem>

#include "battery_monitor.hpp"
#include "config.hpp"
#include "hotplug_scheduler.hpp"
#include "logger.hpp"
#include "platform/single_instance.hpp"
#include "ui/app_window.hpp"
#include "ui/context_menu.hpp"
#include "ui/icon_loader.hpp"
#include "ui/notification_manager.hpp"
#include "ui/tray_icon.hpp"

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

    void setBuildInfo(const char *date, const char *hash) { window.setBuildInfo(date, hash); }

    int run(HINSTANCE instance, WNDPROC wndProc)
    {
        hInstance = instance;

        // Before initialize(), so a second launch never adds a tray icon.
        if (!acquireInstanceLock())
            return 0;

        if (!initialize(wndProc))
            return 1;

        int result = messageLoop();
        shutdown();
        return result;
    }

    void onTrayIconClick() { showContextMenu(); }

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
            // SetTimer repeats; the debounce is one-shot.
            window.killTimer(Constants::ID_TIMER_DEVICE_CHANGE);
            hotplug.onDebounceElapsed();
        }
        else if (timerId == Constants::ID_TIMER_ARRIVAL_RETRY)
        {
            hotplug.onRetryElapsed();
        }
    }

    void onDeviceChange(WPARAM wParam, LPARAM lParam)
    {
        if (wParam != DBT_DEVICEARRIVAL && wParam != DBT_DEVICEREMOVECOMPLETE)
        {
            return;
        }

        PDEV_BROADCAST_HDR hdr = reinterpret_cast<PDEV_BROADCAST_HDR>(lParam);
        if (!hdr || hdr->dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE)
        {
            return;
        }

        hotplug.postEvent(wParam == DBT_DEVICEARRIVAL ? HotplugScheduler::Event::Arrival
                                                      : HotplugScheduler::Event::Removal);
    }

    void onDestroy()
    {
        window.destroy();
        PostQuitMessage(0);
    }

    void onTaskbarCreated() { trayIcon.reAdd(); }

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
    HotplugScheduler hotplug;
    SingleInstanceLock instanceLock;

    bool acquireInstanceLock()
    {
        switch (instanceLock.acquire())
        {
        case SingleInstanceLock::Result::Acquired:
            LOG_DEBUG("Instance lock acquired");
            return true;

        case SingleInstanceLock::Result::AlreadyRunning:
            // Exit 0: one instance running is the desired state.
            LOG_INFO("Another instance is already running - exiting");
            return false;

        case SingleInstanceLock::Result::Unavailable:
            LOG_ERROR("Instance lock unavailable - continuing without a single-instance guard");
            return true;
        }

        return true;
    }

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

    void setAppUserModelID() { SetCurrentProcessExplicitAppUserModelID(L"Mouse Battery Monitor"); }

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

        if (!iconLoader.LoadIcons(resourceDir))
        {
            LOG_ERROR("Failed to load icons from resources folder");
            MessageBoxW(nullptr,
                        L"Failed to load icon resources. Make sure the 'resources' folder exists "
                        L"with required PNG files.",
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

        hotplug.init(&batteryMonitor,
                     {[this](int delayMs)
                      { window.setDeviceChangeTimer(Constants::ID_TIMER_DEVICE_CHANGE, delayMs); },
                      [this]
                      {
                          window.setDeviceChangeTimer(Constants::ID_TIMER_ARRIVAL_RETRY,
                                                      HotplugScheduler::ARRIVAL_RETRY_MS);
                      },
                      [this] { window.killTimer(Constants::ID_TIMER_ARRIVAL_RETRY); }});

        taskbarCreatedMsg = window.registerTaskbarCreatedMessage();
        window.registerDeviceNotifications();
        window.setUpdateTimer(Constants::ID_TIMER_UPDATE, config.GetUpdateIntervalSeconds());

        batteryMonitor.update();
    }

    void showContextMenu()
    {
        ContextMenu menu;
        menu.addItem(Constants::ID_MENU_UPDATE, L"Update Now");
        menu.addItem(Constants::ID_MENU_TRIGGER_LOW_BATTERY, L"Trigger Low Battery",
                     config.GetDebugMode());
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
