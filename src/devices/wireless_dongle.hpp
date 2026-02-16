#pragma once

#include "devices/endgame_gear_device.hpp"

class WirelessDongle : public EndgameGearDevice
{
public:
    static constexpr USHORT PID_DONGLE = 0x1970;

    std::wstring GetDeviceName() const override { return L"Endgame Gear Dongle"; }
    const char *GetDeviceType() const override { return "WirelessDongle"; }
    int GetPriority() const override { return 3; }

protected:
    std::vector<USHORT> GetSupportedPIDs() const override
    {
        return {PID_DONGLE};
    }

    bool IsWiredPID(USHORT) const override
    {
        return false;
    }
};
