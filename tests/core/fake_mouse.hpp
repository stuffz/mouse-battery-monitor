#pragma once

#include "core/device_manager.hpp"
#include "devices/mouse_device.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

enum class Presence
{
    Plugged,
    Unplugged
};

// Mirrors a hidraw device: the handle stays open after an unplug, only the
// reads start failing.
class FakeMouse : public MouseDevice
{
public:
    static constexpr int DEFAULT_PERCENT = 80;

    FakeMouse(std::string type, int priority) : deviceType(std::move(type)), rank(priority) {}

    Presence presence = Presence::Plugged;
    int percent = DEFAULT_PERCENT;
    int failingReads = 0;

    bool FindAndConnect() override
    {
        connected = presence == Presence::Plugged;
        return connected;
    }

    void Disconnect() override { connected = false; }

    bool IsConnected() const override { return connected; }

    BatteryStatus ReadBattery() override
    {
        if (!connected || presence == Presence::Unplugged)
        {
            return {};
        }

        if (failingReads > 0)
        {
            --failingReads;
            return {};
        }

        return {percent, false};
    }

    std::wstring GetDeviceName() const override
    {
        return std::wstring(deviceType.begin(), deviceType.end());
    }

    const char *GetDeviceType() const override { return deviceType.c_str(); }
    int GetPriority() const override { return rank; }
    std::wstring GetConnectionMode() const override { return L"Fake"; }

private:
    std::string deviceType;
    int rank;
    bool connected = false;
};

// The manager owns the fakes; the references add() returns stay valid for as
// long as the manager built from them lives.
class FakeMice
{
public:
    FakeMouse &add(const std::string &type, int priority)
    {
        auto mouse = std::make_unique<FakeMouse>(type, priority);
        FakeMouse &handle = *mouse;
        devices.push_back(std::move(mouse));
        return handle;
    }

    DeviceManager release() { return DeviceManager(std::move(devices)); }

private:
    std::vector<std::unique_ptr<MouseDevice>> devices;
};
