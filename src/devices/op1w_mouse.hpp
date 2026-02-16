#pragma once

#include "devices/endgame_gear_device.hpp"

class OP1WMouse : public EndgameGearDevice
{
public:
    static constexpr USHORT PID_WIRED = 0x1972;

    std::wstring GetDeviceName() const override { return L"OP1W"; }
    const char *GetDeviceType() const override { return "OP1WMouse"; }
    int GetPriority() const override { return 1; }

protected:
    std::vector<USHORT> GetSupportedPIDs() const override
    {
        return {PID_WIRED};
    }

    bool IsWiredPID(USHORT pid) const override
    {
        return pid == PID_WIRED;
    }
};
