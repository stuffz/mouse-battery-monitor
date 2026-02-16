#pragma once

#include <windows.h>
#include <string>
#include <array>
#include <memory>
#include <gdiplus.h>
#include "core/logger.hpp"

using std::wstring;
using std::string;

class IconLoader
{
public:
    IconLoader() : disconnectedIcon(nullptr), gdiplusToken(0)
    {
        batteryIcons.fill(nullptr);
        chargingIcons.fill(nullptr);
        InitializeGdiPlus();
    }

    ~IconLoader()
    {
        CleanupIcons();
        ShutdownGdiPlus();
    }

    IconLoader(const IconLoader&) = delete;
    IconLoader& operator=(const IconLoader&) = delete;

    bool LoadIcons(const wstring& resourcePath)
    {
        LOG_DEBUG("Loading icons from: " + WStringToString(resourcePath));

        bool allLoaded = true;
        allLoaded &= LoadBatteryAndChargingIcons(resourcePath);
        allLoaded &= LoadDisconnectedIcon(resourcePath);

        if (!allLoaded)
        {
            LOG_ERROR("One or more icons failed to load");
            return false;
        }

        LOG_INFO("Icons loaded successfully");
        return true;
    }

    HICON GetBatteryIcon(int percentage, bool isCharging) const
    {
        if (percentage < 0)
        {
            return disconnectedIcon;
        }

        const int index = std::clamp((percentage + 5) / 10, 0, NUM_LEVELS - 1);
        return isCharging ? chargingIcons[index] : batteryIcons[index];
    }

    HICON GetDisconnectedIcon() const
    {
        return disconnectedIcon;
    }

private:
    static constexpr int NUM_LEVELS = 11;

    std::array<HICON, NUM_LEVELS> batteryIcons;
    std::array<HICON, NUM_LEVELS> chargingIcons;
    HICON disconnectedIcon;
    ULONG_PTR gdiplusToken;

    void InitializeGdiPlus()
    {
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
    }

    void ShutdownGdiPlus()
    {
        if (gdiplusToken)
        {
            Gdiplus::GdiplusShutdown(gdiplusToken);
        }
    }

    bool LoadBatteryAndChargingIcons(const wstring& resourcePath)
    {
        bool allLoaded = true;

        for (int i = 0; i < NUM_LEVELS; ++i)
        {
            const int percentage = i * 10;

            batteryIcons[i] = LoadIconFromPNG(BuildIconPath(resourcePath, L"battery", percentage));
            if (!batteryIcons[i])
            {
                LOG_ERROR("Missing or invalid battery icon: " + std::to_string(percentage) + "%");
                allLoaded = false;
            }

            chargingIcons[i] = LoadIconFromPNG(BuildIconPath(resourcePath, L"charging", percentage));
            if (!chargingIcons[i])
            {
                LOG_ERROR("Missing or invalid charging icon: " + std::to_string(percentage) + "%");
                allLoaded = false;
            }
        }

        return allLoaded;
    }

    bool LoadDisconnectedIcon(const wstring& resourcePath)
    {
        disconnectedIcon = LoadIconFromPNG(resourcePath + L"\\disconnected.png");
        if (!disconnectedIcon)
        {
            LOG_ERROR("Missing or invalid disconnected icon");
            return false;
        }
        return true;
    }

    static wstring BuildIconPath(const wstring& basePath, const wstring& iconType, int percentage)
    {
        return basePath + L"\\" + iconType + L"_" + std::to_wstring(percentage) + L".png";
    }

    HICON LoadIconFromPNG(const wstring& filename)
    {
        std::unique_ptr<Gdiplus::Bitmap> bitmap(new Gdiplus::Bitmap(filename.c_str()));

        if (bitmap->GetLastStatus() != Gdiplus::Ok)
        {
            LOG_ERROR("Failed to load icon: " + WStringToString(filename));
            return nullptr;
        }

        HICON icon = nullptr;
        bitmap->GetHICON(&icon);
        return icon;
    }

    void CleanupIcons()
    {
        for (HICON icon : batteryIcons)
        {
            if (icon)
                DestroyIcon(icon);
        }

        for (HICON icon : chargingIcons)
        {
            if (icon)
                DestroyIcon(icon);
        }

        if (disconnectedIcon)
        {
            DestroyIcon(disconnectedIcon);
        }
    }

    static string WStringToString(const wstring& wstr)
    {
        return string(wstr.begin(), wstr.end());
    }
};
