#pragma once

#include <string>

class MouseDevice
{
public:
    struct BatteryStatus
    {
        int percentage{-1};
        bool isCharging{false};
        bool isWireless{false};
    };

    virtual ~MouseDevice() = default;

    MouseDevice(const MouseDevice &) = delete;
    MouseDevice &operator=(const MouseDevice &) = delete;

    virtual bool FindAndConnect() = 0;
    virtual void Disconnect() = 0;
    virtual bool IsConnected() const = 0;
    virtual BatteryStatus ReadBattery() = 0;

    virtual std::wstring GetDeviceName() const = 0;
    virtual const char *GetDeviceType() const = 0;
    virtual int GetPriority() const = 0;
    virtual std::wstring GetConnectionMode() const = 0;

protected:
    MouseDevice() = default;
};
