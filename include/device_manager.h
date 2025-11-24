#pragma once

#include "endgame_gear_device.h"
#include "op1w_mouse.h"
#include "xm2w_mouse.h"
#include "op1w_dongle.h"
#include "logger.h"
#include <memory>
#include <vector>
#include <algorithm>

class DeviceManager
{
public:
    using BatteryStatus = EndgameGearDevice::BatteryStatus;

    DeviceManager()
    {
        devices.push_back(std::make_unique<OP1WMouse>());
        devices.push_back(std::make_unique<XM2Wv2Mouse>());
        devices.push_back(std::make_unique<WirelessDongle>());

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
                active_device = device.get();
                LOG(LogLevel::Info, std::string("Active device: ") + device->GetDeviceType());
                return true;
            }
        }
        return false;
    }

    void Disconnect()
    {
        if (active_device)
        {
            active_device->Disconnect();
            active_device = nullptr;
        }
    }

    bool IsConnected() const
    {
        return active_device && active_device->IsConnected();
    }

    BatteryStatus ReadBattery()
    {
        if (!active_device)
        {
            return {};
        }
        return active_device->ReadBattery();
    }

    std::wstring GetDeviceName() const
    {
        return active_device ? active_device->GetDeviceName() : L"Unknown";
    }

    std::wstring GetConnectionMode() const
    {
        return active_device ? active_device->GetConnectionMode() : L"Unknown";
    }

    bool ShouldSwitchDevice()
    {
        if (!active_device)
        {
            return false;
        }

        int current_priority = active_device->GetPriority();

        for (auto &device : devices)
        {
            if (device.get() == active_device)
            {
                continue;
            }

            if (device->GetPriority() < current_priority)
            {
                if (device->FindAndConnect())
                {
                    LOG(LogLevel::Info, std::string("Switching to higher priority device: ") +
                                            device->GetDeviceType());
                    active_device->Disconnect();
                    active_device = device.get();
                    return true;
                }
            }
        }

        return false;
    }

private:
    std::vector<std::unique_ptr<EndgameGearDevice>> devices;
    EndgameGearDevice *active_device = nullptr;
};
