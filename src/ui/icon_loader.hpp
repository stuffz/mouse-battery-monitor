#pragma once

#include "core/logger.hpp"
#include "ui/icon_backend.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <string>

class IconLoader
{
public:
    IconLoader() = default;

    ~IconLoader() { Clear(); }

    IconLoader(const IconLoader &) = delete;
    IconLoader &operator=(const IconLoader &) = delete;

    bool LoadIcons(const std::filesystem::path &resourcePath)
    {
        LOG_DEBUG("Loading icons from: " + resourcePath.u8string());

        bool allLoaded = true;

        for (int i = 0; i < NUM_LEVELS; ++i)
        {
            const int percentage = i * 10;

            allLoaded &= LoadLevel(resourcePath, "battery", percentage, batteryIcons[i]);
            allLoaded &= LoadLevel(resourcePath, "charging", percentage, chargingIcons[i]);
        }

        if (!Load(resourcePath / "disconnected.png", disconnectedIcon))
        {
            LOG_ERROR("Missing or invalid disconnected icon");
            allLoaded = false;
        }

        if (!allLoaded)
        {
            LOG_ERROR("One or more icons failed to load");
            return false;
        }

        LOG_INFO("Icons loaded successfully");
        return true;
    }

    const NativeIcon &GetBatteryIcon(int percentage, bool isCharging) const
    {
        if (percentage < 0)
        {
            return disconnectedIcon;
        }

        const int index = std::clamp((percentage + 5) / 10, 0, NUM_LEVELS - 1);
        return isCharging ? chargingIcons[index] : batteryIcons[index];
    }

    const NativeIcon &GetDisconnectedIcon() const { return disconnectedIcon; }

    void Clear()
    {
        for (auto &icon : batteryIcons)
        {
            IconBackend::Destroy(icon);
        }

        for (auto &icon : chargingIcons)
        {
            IconBackend::Destroy(icon);
        }

        IconBackend::Destroy(disconnectedIcon);
    }

private:
    static constexpr int NUM_LEVELS = 11;

    // Declared before the icons so it is destroyed after them: on Windows the
    // handles must be released while GDI+ is still initialised.
    IconBackend backend;

    std::array<NativeIcon, NUM_LEVELS> batteryIcons{};
    std::array<NativeIcon, NUM_LEVELS> chargingIcons{};
    NativeIcon disconnectedIcon{};

    bool LoadLevel(const std::filesystem::path &resourcePath, const std::string &iconType,
                   int percentage, NativeIcon &target)
    {
        const auto path = resourcePath / (iconType + "_" + std::to_string(percentage) + ".png");

        if (!Load(path, target))
        {
            LOG_ERROR("Missing or invalid " + iconType + " icon: " + std::to_string(percentage) +
                      "%");
            return false;
        }

        return true;
    }

    bool Load(const std::filesystem::path &path, NativeIcon &target)
    {
        if (!backend.Load(path, target))
        {
            LOG_ERROR("Failed to load icon: " + path.u8string());
            return false;
        }

        return true;
    }
};
