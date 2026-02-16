#pragma once

#include "devices/mouse_device.hpp"
#include "devices/endgame_gear_device.hpp"
#include "devices/op1w_mouse.hpp"
#include "devices/xm2w_mouse.hpp"
#include "devices/wireless_dongle.hpp"
#include "devices/vaxee_device.hpp"
#include "devices/vaxee_mouse.hpp"
#include "devices/vaxee_dongle.hpp"
#include "core/logger.hpp"
#include <memory>
#include <vector>
#include <algorithm>
#include <string>

using std::string;
using std::unique_ptr;
using std::vector;
using std::wstring;

class DeviceManager
{
public:
    using BatteryStatus = MouseDevice::BatteryStatus;

    DeviceManager()
    {
        // Endgame Gear devices
        devices.push_back(std::make_unique<OP1WMouse>());
        devices.push_back(std::make_unique<XM2Wv2Mouse>());
        devices.push_back(std::make_unique<WirelessDongle>());

        // VAXEE devices
        devices.push_back(std::make_unique<VaxeeMouse>());
        devices.push_back(std::make_unique<VaxeeDongle>());

        std::sort(devices.begin(), devices.end(),
                  [](const auto &a, const auto &b)
                  {
                      return a->GetPriority() < b->GetPriority();
                  });
    }

    bool FindAndConnect()
    {
        for (auto &device : devices)
        {
            if (device->FindAndConnect())
            {
                activeDevice = device.get();
                LOG_INFO(string("Active device: ") + device->GetDeviceType());
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

    bool IsConnected() const
    {
        return activeDevice && activeDevice->IsConnected();
    }

    BatteryStatus ReadBattery()
    {
        if (!activeDevice)
        {
            return {};
        }
        return activeDevice->ReadBattery();
    }

    wstring GetDeviceName() const
    {
        return activeDevice ? activeDevice->GetDeviceName() : L"Unknown";
    }

    wstring GetConnectionMode() const
    {
        return activeDevice ? activeDevice->GetConnectionMode() : L"Unknown";
    }

    bool ShouldSwitchDevice()
    {
        if (!activeDevice)
        {
            return false;
        }

        int currentPriority = activeDevice->GetPriority();

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
                    LOG_INFO(string("Switching to higher priority device: ") +
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
    vector<unique_ptr<MouseDevice>> devices;
    MouseDevice *activeDevice = nullptr;
};
