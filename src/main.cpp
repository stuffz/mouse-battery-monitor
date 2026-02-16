#include <windows.h>
#include <string>
#include "core/application.hpp"
#include "core/logger.hpp"

#ifndef BUILD_DATE
#define BUILD_DATE "unknown"
#endif

#ifndef GIT_HASH
#define GIT_HASH "unknown"
#endif

using std::string;
using Constants = Application::Constants;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto &app = Application::instance();

    switch (msg)
    {
    case WM_CREATE:
        return 0;

    case Constants::WM_TRAYICON:
        if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU)
        {
            app.onTrayIconClick();
        }
        return 0;

    case WM_COMMAND:
        app.onMenuCommand(LOWORD(wParam));
        return 0;

    case WM_TIMER:
        app.onTimer(wParam);
        return 0;

    case WM_DEVICECHANGE:
        app.onDeviceChange(wParam, lParam);
        return 0;

    case WM_DESTROY:
        app.onDestroy();
        return 0;

    default:
        if (app.getTaskbarCreatedMsg() != 0 && msg == app.getTaskbarCreatedMsg())
        {
            app.onTaskbarCreated();
            return 0;
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    try
    {
        auto &app = Application::instance();
        app.setBuildInfo(BUILD_DATE, GIT_HASH);
        return app.run(hInstance, WndProc);
    }
    catch (const std::exception &ex)
    {
        LOG_ERROR(string("Unhandled exception: ") + ex.what());
        MessageBoxW(nullptr, L"Unhandled exception. Check log for details.", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    catch (...)
    {
        LOG_ERROR("Unhandled unknown exception");
        MessageBoxW(nullptr, L"Unhandled exception. Check log for details.", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
}
