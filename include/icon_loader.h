#pragma once

#include <windows.h>
#include <string>
#include <array>
#include <memory>
#include <gdiplus.h>
#include "logger.h"

class IconLoader
{
public:
    IconLoader() : disconnected_icon(nullptr), gdiplusToken(0)
    {
        battery_icons.fill(nullptr);
        charging_icons.fill(nullptr);
        InitializeGdiPlus();
    }

    ~IconLoader()
    {
        CleanupIcons();
        ShutdownGdiPlus();
    }

    IconLoader(const IconLoader &) = delete;
    IconLoader &operator=(const IconLoader &) = delete;

    bool LoadIcons(const std::wstring &resource_path)
    {
        LOG(LogLevel::Debug, "Loading icons from: " + WStringToString(resource_path));

        bool all_loaded = true;
        all_loaded &= LoadBatteryAndChargingIcons(resource_path);
        all_loaded &= LoadDisconnectedIcon(resource_path);

        if (!all_loaded)
        {
            LOG(LogLevel::Error, "One or more icons failed to load");
            return false;
        }

        LOG(LogLevel::Info, "Icons loaded successfully");
        return true;
    }

    HICON GetBatteryIcon(int percentage, bool is_charging) const
    {
        if (percentage < 0)
        {
            return disconnected_icon;
        }

        const int index = std::clamp((percentage + 5) / 10, 0, NUM_LEVELS - 1);
        return is_charging ? charging_icons[index] : battery_icons[index];
    }

    HICON GetDisconnectedIcon() const
    {
        return disconnected_icon;
    }

private:
    static constexpr int NUM_LEVELS = 11; // 0%, 10%, 20%, ..., 100%

    std::array<HICON, NUM_LEVELS> battery_icons;
    std::array<HICON, NUM_LEVELS> charging_icons;
    HICON disconnected_icon;
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

    bool LoadBatteryAndChargingIcons(const std::wstring &resource_path)
    {
        bool all_loaded = true;

        for (int i = 0; i < NUM_LEVELS; ++i)
        {
            const int percentage = i * 10;

            battery_icons[i] = LoadIconFromPNG(BuildIconPath(resource_path, L"battery", percentage));
            if (!battery_icons[i])
            {
                LOG(LogLevel::Error, "Missing or invalid battery icon: " + std::to_string(percentage) + "%");
                all_loaded = false;
            }

            charging_icons[i] = LoadIconFromPNG(BuildIconPath(resource_path, L"charging", percentage));
            if (!charging_icons[i])
            {
                LOG(LogLevel::Error, "Missing or invalid charging icon: " + std::to_string(percentage) + "%");
                all_loaded = false;
            }
        }

        return all_loaded;
    }

    bool LoadDisconnectedIcon(const std::wstring &resource_path)
    {
        disconnected_icon = LoadIconFromPNG(resource_path + L"\\disconnected.png");
        if (!disconnected_icon)
        {
            LOG(LogLevel::Error, "Missing or invalid disconnected icon");
            return false;
        }
        return true;
    }

    static std::wstring BuildIconPath(const std::wstring &base_path, const std::wstring &icon_type, int percentage)
    {
        return base_path + L"\\" + icon_type + L"_" + std::to_wstring(percentage) + L".png";
    }

    HICON LoadIconFromPNG(const std::wstring &filename)
    {
        std::unique_ptr<Gdiplus::Bitmap> bitmap(new Gdiplus::Bitmap(filename.c_str()));

        if (bitmap->GetLastStatus() != Gdiplus::Ok)
        {
            LOG(LogLevel::Error, "Failed to load icon: " + WStringToString(filename));
            return nullptr;
        }

        HICON hIcon = nullptr;
        bitmap->GetHICON(&hIcon);
        return hIcon;
    }

    void CleanupIcons()
    {
        for (HICON icon : battery_icons)
        {
            if (icon)
                DestroyIcon(icon);
        }

        for (HICON icon : charging_icons)
        {
            if (icon)
                DestroyIcon(icon);
        }

        if (disconnected_icon)
        {
            DestroyIcon(disconnected_icon);
        }
    }

    static std::string WStringToString(const std::wstring &wstr)
    {
        return std::string(wstr.begin(), wstr.end());
    }
};
