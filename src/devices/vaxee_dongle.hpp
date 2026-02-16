#pragma once

#include "devices/vaxee_device.hpp"

class VaxeeDongle : public VaxeeDevice
{
public:
    static constexpr USHORT PID_DONGLE_1 = 0x1001;
    static constexpr USHORT PID_DONGLE_2 = 0x1002;
    static constexpr USHORT PID_DONGLE_3 = 0x0005;
    static constexpr USHORT PID_DONGLE_4K = 0x2001;

    std::wstring GetDeviceName() const override
    {
        return GetNameForPID(currentPid);
    }

    const char *GetDeviceType() const override { return "VaxeeDongle"; }
    int GetPriority() const override { return 5; }

protected:
    std::vector<USHORT> GetSupportedPIDs() const override
    {
        return {PID_DONGLE_1, PID_DONGLE_2, PID_DONGLE_3, PID_DONGLE_4K};
    }

    bool IsDonglePID(USHORT) const override
    {
        return true;
    }

private:
    static std::wstring GetNameForPID(USHORT pid)
    {
        switch (pid)
        {
        case PID_DONGLE_1:
            return L"VAXEE Dongle";
        case PID_DONGLE_2:
            return L"VAXEE 4K Dongle";
        case PID_DONGLE_3:
            return L"VAXEE Dongle";
        case PID_DONGLE_4K:
            return L"VAXEE 4K Dongle (Dual-track)";
        default:
            return L"VAXEE Dongle";
        }
    }
};
