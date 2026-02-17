#pragma once

#include "devices/endgame_gear_device.hpp"

class EndgameGearMouse : public EndgameGearDevice
{
public:
    std::wstring GetDeviceName() const override
    {
        return GetNameForPID(currentPid);
    }

    const char *GetDeviceType() const override { return "EndgameGearMouse"; }
    int GetPriority() const override { return 1; }

protected:
    std::vector<USHORT> GetSupportedPIDs() const override
    {
        return {
            0x1972, // OP1W
            0x1982, // XM2W v2
        };
    }

    bool IsWiredPID(USHORT) const override
    {
        return true;
    }

private:
    static std::wstring GetNameForPID(USHORT pid)
    {
        switch (pid)
        {
        case 0x1972:
            return L"OP1W";
        case 0x1982:
            return L"XM2W v2";
        default:
            return L"Endgame Gear Mouse";
        }
    }
};
