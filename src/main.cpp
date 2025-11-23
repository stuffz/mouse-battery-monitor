#include <windows.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <propkey.h>
#include <propvarutil.h>
#include <string>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <limits>
#include <exception>
#include "config.h"
#include "op1w_device.h"
#include "icon_loader.h"
#include "logger.h"

#ifndef BUILD_DATE
#define BUILD_DATE "unknown"
#endif

#ifndef GIT_HASH
#define GIT_HASH "unknown"
#endif

namespace fs = std::filesystem;

namespace Constants
{
    constexpr UINT WM_TRAYICON = WM_USER + 1;
    constexpr UINT ID_TRAY_ICON = 1;
    constexpr UINT ID_TIMER_UPDATE = 1;
    constexpr UINT ID_MENU_UPDATE = 1001;
    constexpr UINT ID_MENU_TRIGGER_LOW_BATTERY = 1002;
    constexpr UINT ID_MENU_ABOUT = 1003;
    constexpr UINT ID_MENU_EXIT = 1004;
    constexpr wchar_t WINDOW_CLASS[] = L"EndgameGearMouseBatteryMonitorClass";
    constexpr wchar_t WINDOW_TITLE[] = L"Endgame Gear Mouse Battery Monitor";
    constexpr wchar_t CONFIG_FILE[] = L"config.ini";
    constexpr wchar_t LOG_FILE[] = L"battery_monitor.log";
}

struct AppContext
{
    HINSTANCE hInst{nullptr};
    HWND hwnd{nullptr};
    NOTIFYICONDATAW nid{};
    Config config;
    OP1WDevice device;
    IconLoader icon_loader;
};

static AppContext g_app;

// Function declarations
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void SetAppUserModelID();
static fs::path GetResourceDirectory();
static bool InitializeConfig();
static void SetupDebugConsole();
static bool LoadIconResources();
static bool RegisterWindowClass(HINSTANCE hInstance);
static HWND CreateMessageWindow(HINSTANCE hInstance);
static void InitTrayIcon(HWND hwnd);
static void UpdateBatteryStatus();
static void ShowContextMenu(HWND hwnd);
static void ShowAboutDialog(HWND hwnd);
static void TriggerLowBatteryNotification();
static void CleanupTrayIcon();
static void WaitForExitIfDebug();
static std::wstring BuildTooltip(const OP1WDevice::BatteryStatus &status);
static void HandleLowBatteryNotification(const OP1WDevice::BatteryStatus &status);

static void SetAppUserModelID()
{
    // Set a custom app ID so notifications show "Endgame Gear Mouse Battery Monitor" instead of the .exe name
    SetCurrentProcessExplicitAppUserModelID(L"Endgame Gear - Mouse Battery Monitor");
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, int /*nCmdShow*/)
{
    try
    {
        SetAppUserModelID();

        LOG(LogLevel::Debug, "Entered WinMain");
        g_app.hInst = hInstance;
        if (!InitializeConfig())
        {
            WaitForExitIfDebug();
            return 1;
        }

        SetupDebugConsole();
        LOG(LogLevel::Info, "Starting Endgame Gear Mouse Battery Monitor");

        if (!LoadIconResources())
        {
            WaitForExitIfDebug();
            return 1;
        }

        if (!RegisterWindowClass(hInstance))
        {
            MessageBoxW(nullptr, L"Failed to register window class", L"Error", MB_OK | MB_ICONERROR);
            WaitForExitIfDebug();
            return 1;
        }

        g_app.hwnd = CreateMessageWindow(hInstance);
        if (!g_app.hwnd)
        {
            MessageBoxW(nullptr, L"Failed to create window", L"Error", MB_OK | MB_ICONERROR);
            WaitForExitIfDebug();
            return 1;
        }

        InitTrayIcon(g_app.hwnd);
        SetTimer(g_app.hwnd, Constants::ID_TIMER_UPDATE, g_app.config.GetUpdateIntervalSeconds() * 1000, nullptr);
        LOG(LogLevel::Debug, "Update timer set for " + std::to_string(g_app.config.GetUpdateIntervalSeconds()) + " seconds");

        UpdateBatteryStatus();

        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        CleanupTrayIcon();
        LOG(LogLevel::Info, "Shutting down");
        WaitForExitIfDebug();
        return static_cast<int>(msg.wParam);
    }
    catch (const std::exception &ex)
    {
        LOG(LogLevel::Error, std::string("Unhandled exception: ") + ex.what());
        MessageBoxW(nullptr, L"Unhandled exception. Check log for details.", L"Error", MB_OK | MB_ICONERROR);
        WaitForExitIfDebug();
        return 1;
    }
    catch (...)
    {
        LOG(LogLevel::Error, "Unhandled unknown exception");
        MessageBoxW(nullptr, L"Unhandled exception. Check log for details.", L"Error", MB_OK | MB_ICONERROR);
        WaitForExitIfDebug();
        return 1;
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        return 0;

    case Constants::WM_TRAYICON:
        if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU)
        {
            ShowContextMenu(hwnd);
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case Constants::ID_MENU_UPDATE:
            UpdateBatteryStatus();
            break;
        case Constants::ID_MENU_TRIGGER_LOW_BATTERY:
            TriggerLowBatteryNotification();
            break;
        case Constants::ID_MENU_ABOUT:
            ShowAboutDialog(hwnd);
            break;
        case Constants::ID_MENU_EXIT:
            PostQuitMessage(0);
            break;
        }
        return 0;

    case WM_TIMER:
        if (wParam == Constants::ID_TIMER_UPDATE)
        {
            UpdateBatteryStatus();
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

static void InitTrayIcon(HWND hwnd)
{
    LOG(LogLevel::Debug, "Initializing tray icon");

    g_app.nid = {};
    g_app.nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_app.nid.hWnd = hwnd;
    g_app.nid.uID = Constants::ID_TRAY_ICON;
    g_app.nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_app.nid.uCallbackMessage = Constants::WM_TRAYICON;
    g_app.nid.hIcon = g_app.icon_loader.GetDisconnectedIcon();
    wcscpy_s(g_app.nid.szTip, L"Endgame Gear Mouse Battery Monitor\nNo device connected");

    if (!Shell_NotifyIconW(NIM_ADD, &g_app.nid))
    {
        const DWORD err = GetLastError();
        LOG(LogLevel::Error, "Shell_NotifyIconW failed: " + std::to_string(err));
        MessageBoxW(nullptr, L"Failed to add tray icon. Check that icon files exist in the resources folder.", L"Error", MB_OK | MB_ICONERROR);
        PostQuitMessage(1);
    }
    else
    {
        LOG(LogLevel::Debug, "Tray icon added successfully");
    }
}

static void UpdateBatteryStatus()
{
    LOG(LogLevel::Debug, "Updating battery status");

    if (!g_app.device.IsConnected())
    {
        LOG(LogLevel::Debug, "Device not connected, attempting to find and connect");
        if (g_app.device.FindAndConnect())
        {
            LOG(LogLevel::Info, "Device connected successfully");
        }
    }

    try
    {
        const auto status = g_app.device.ReadBattery();

        if (status.percentage < 0)
        {
            LOG(LogLevel::Debug, "No device connected or read error - attempting reconnection");
            // Device might have switched modes (wired/wireless), try to reconnect
            g_app.device.Disconnect();
            if (g_app.device.FindAndConnect())
            {
                LOG(LogLevel::Info, "Device reconnected after mode switch");
            }

            g_app.nid.hIcon = g_app.icon_loader.GetDisconnectedIcon();
            wcscpy_s(g_app.nid.szTip, L"Endgame Gear Mouse Battery Monitor\nNo device connected");
        }
        else
        {
            LOG(LogLevel::Debug, "Battery: " + std::to_string(status.percentage) + "%, Charging: " +
                                     (status.is_charging ? "Yes" : "No"));
            g_app.nid.hIcon = g_app.icon_loader.GetBatteryIcon(status.percentage, status.is_charging);
            wcscpy_s(g_app.nid.szTip, BuildTooltip(status).c_str());

            HandleLowBatteryNotification(status);
        }

        Shell_NotifyIconW(NIM_MODIFY, &g_app.nid);
        g_app.nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    }
    catch (const std::exception &ex)
    {
        LOG(LogLevel::Error, "Exception in UpdateBatteryStatus: " + std::string(ex.what()));
        g_app.device.Disconnect();
    }
    catch (...)
    {
        LOG(LogLevel::Error, "Unknown exception in UpdateBatteryStatus");
        g_app.device.Disconnect();
    }
}

static void ShowContextMenu(HWND hwnd)
{
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, Constants::ID_MENU_UPDATE, L"Update Now");

    if (g_app.config.GetDebugMode())
    {
        AppendMenuW(hMenu, MF_STRING, Constants::ID_MENU_TRIGGER_LOW_BATTERY, L"Trigger Low Battery");
    }

    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, Constants::ID_MENU_ABOUT, L"About");
    AppendMenuW(hMenu, MF_STRING, Constants::ID_MENU_EXIT, L"Exit");

    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(hMenu);
}

static void ShowAboutDialog(HWND hwnd)
{
    std::wstringstream about;
    about << L"Endgame Gear Mouse Battery Monitor\n\n"
          << L"Version: 1.0\n"
          << L"Build Date: " << BUILD_DATE << L"\n"
          << L"Git Hash: " << GIT_HASH << L"\n\n"
          << L"Monitors battery status for Endgame Gear wireless mice.\n\n"
          << L"Update Interval: " << g_app.config.GetUpdateIntervalSeconds() << L" seconds\n"
          << L"Low Battery Threshold: " << g_app.config.GetLowBatteryThreshold() << L"%\n"
          << L"Debug Mode: " << (g_app.config.GetDebugMode() ? L"Enabled" : L"Disabled");

    MessageBoxW(hwnd, about.str().c_str(), L"About Endgame Gear Mouse Battery Monitor", MB_OK | MB_ICONINFORMATION);
}

static void CleanupTrayIcon()
{
    Shell_NotifyIconW(NIM_DELETE, &g_app.nid);
    g_app.device.Disconnect();
}

static void WaitForExitIfDebug()
{
    if (!g_app.config.GetDebugMode())
    {
        return;
    }

    std::cout << "\nPress Enter to exit..." << std::flush;
    std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
}

static fs::path GetResourceDirectory()
{
    wchar_t exe_path[MAX_PATH];
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    fs::path exe_dir = fs::path(exe_path).parent_path();
    fs::path resource_dir = exe_dir / L"resources";

    LOG(LogLevel::Debug, "Executable directory: " + exe_dir.u8string());
    LOG(LogLevel::Debug, "Resource directory: " + resource_dir.u8string());

    return resource_dir;
}

static bool InitializeConfig()
{
    if (!g_app.config.Load("config.ini"))
    {
        LOG(LogLevel::Error, "Failed to load config.ini, using defaults");
    }
    else
    {
        LOG(LogLevel::Debug, "Loaded configuration from config.ini");
    }

    Logger::Instance().SetDebugMode(g_app.config.GetDebugMode());
    Logger::Instance().SetLogFile("battery_monitor.log");
    LOG(LogLevel::Debug, "Logger configured");

    return true;
}

static void SetupDebugConsole()
{
    if (g_app.config.GetDebugMode())
    {
        AllocConsole();
        FILE *fp = nullptr;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        freopen_s(&fp, "CONIN$", "r", stdin);
        LOG(LogLevel::Info, "Debug mode enabled");
    }
}

static bool LoadIconResources()
{
    const auto resource_dir = GetResourceDirectory();

    if (!g_app.icon_loader.LoadIcons(resource_dir.wstring()))
    {
        LOG(LogLevel::Error, "Failed to load icons from resources folder");
        MessageBoxW(nullptr, L"Failed to load icon resources. Make sure the 'resources' folder exists with required PNG files.",
                    L"Error", MB_OK | MB_ICONERROR);
        return false;
    }

    LOG(LogLevel::Debug, "Icon load succeeded");
    return true;
}

static bool RegisterWindowClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = Constants::WINDOW_CLASS;

    return RegisterClassExW(&wc) != 0;
}

static HWND CreateMessageWindow(HINSTANCE hInstance)
{
    HWND hwnd = CreateWindowExW(
        0,
        Constants::WINDOW_CLASS,
        Constants::WINDOW_TITLE,
        0,
        0, 0, 0, 0,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if (hwnd)
    {
        LOG(LogLevel::Debug, "Hidden window created for message loop");
    }

    return hwnd;
}

static std::wstring BuildTooltip(const OP1WDevice::BatteryStatus &status)
{
    std::wstringstream tooltip;
    tooltip << L"OP1W\n"
            << g_app.device.GetConnectionMode() << L"\n"
            << L"Battery: " << status.percentage << L"%";
    return tooltip.str();
}

static void HandleLowBatteryNotification(const OP1WDevice::BatteryStatus &status)
{
    if (!g_app.config.GetShowNotifications() || status.percentage > g_app.config.GetLowBatteryThreshold() || status.percentage <= 0)
    {
        return;
    }

    static bool notification_shown = false;

    if (!notification_shown)
    {
        std::wstringstream title;
        title << L"Endgame Gear " << g_app.device.GetDeviceName() << L" - Low Battery";

        g_app.nid.uFlags |= NIF_INFO;
        wcscpy_s(g_app.nid.szInfoTitle, title.str().c_str());

        std::wstringstream msg;
        msg << L"Battery at " << status.percentage << L"%";
        wcscpy_s(g_app.nid.szInfo, msg.str().c_str());

        g_app.nid.dwInfoFlags = NIIF_WARNING;
        notification_shown = true;
    }

    if (status.percentage > g_app.config.GetLowBatteryThreshold() + 5)
    {
        notification_shown = false;
    }
}

static void TriggerLowBatteryNotification()
{
    LOG(LogLevel::Info, "Triggering low battery notification");

    const auto status = g_app.device.ReadBattery();
    const int battery_percentage = status.percentage >= 0 ? status.percentage : g_app.config.GetLowBatteryThreshold();

    std::wstringstream title;
    if (g_app.device.IsConnected())
    {
        title << L"Endgame Gear " << g_app.device.GetDeviceName() << L" - Low Battery";
    }
    else
    {
        title << L"Endgame Gear Mouse - Low Battery";
    }

    NOTIFYICONDATAW nid = g_app.nid;
    nid.uFlags |= NIF_INFO;
    wcscpy_s(nid.szInfoTitle, title.str().c_str());

    std::wstringstream msg;
    msg << L"Battery at " << battery_percentage << L"%";
    wcscpy_s(nid.szInfo, msg.str().c_str());

    nid.dwInfoFlags = NIIF_WARNING;

    Shell_NotifyIconW(NIM_MODIFY, &nid);
}
