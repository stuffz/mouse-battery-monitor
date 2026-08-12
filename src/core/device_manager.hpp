#pragma once

#include "core/logger.hpp"
#include "devices/endgame_gear_dongle.hpp"
#include "devices/endgame_gear_mouse.hpp"
#include "devices/mouse_device.hpp"
#include "devices/vaxee_dongle.hpp"
#include "devices/vaxee_mouse.hpp"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

class DeviceManager
{
public:
    using BatteryStatus = MouseDevice::BatteryStatus;

    DeviceManager()
    {
        devices.push_back(std::make_unique<EndgameGearMouse>());
        devices.push_back(std::make_unique<EndgameGearDongle>());

        devices.push_back(std::make_unique<VaxeeMouse>());
        devices.push_back(std::make_unique<VaxeeDongle>());

        std::sort(devices.begin(), devices.end(),
                  [](const auto &a, const auto &b) { return a->GetPriority() < b->GetPriority(); });
    }

    bool FindAndConnect()
    {
        for (auto &device : devices)
        {
            if (device->FindAndConnect())
            {
                activeDevice = device.get();
                LOG_INFO(std::string("Active device: ") + device->GetDeviceType());
                return true;
            }
        }
        return false;
    }

    void Disconnect()
    {
        if (activeDevice)
        {
            activeDevice->Disconnect();
            activeDevice = nullptr;
        }
    }

    bool IsConnected() const { return activeDevice && activeDevice->IsConnected(); }

    BatteryStatus ReadBattery()
    {
        if (!activeDevice)
        {
            return {};
        }
        return activeDevice->ReadBattery();
    }

    std::wstring GetDeviceName() const
    {
        return activeDevice ? activeDevice->GetDeviceName() : L"Unknown";
    }

    std::wstring GetConnectionMode() const
    {
        return activeDevice ? activeDevice->GetConnectionMode() : L"Unknown";
    }

    bool ShouldSwitchDevice()
    {
        if (!activeDevice)
        {
            return false;
        }

        const int currentPriority = activeDevice->GetPriority();

        for (auto &device : devices)
        {
            if (device.get() == activeDevice)
            {
                continue;
            }

            if (device->GetPriority() < currentPriority)
            {
                if (device->FindAndConnect())
                {
                    LOG_INFO(std::string("Switching to higher priority device: ") +
                             device->GetDeviceType());
                    activeDevice->Disconnect();
                    activeDevice = device.get();
                    return true;
                }
            }
        }

        return false;
    }

private:
    std::vector<std::unique_ptr<MouseDevice>> devices;
    MouseDevice *activeDevice = nullptr;
};
