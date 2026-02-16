#pragma once

#include "devices/endgame_gear_device.hpp"

class XM2Wv2Mouse : public EndgameGearDevice
{
public:
    static constexpr USHORT PID_WIRED = 0x1982;

    std::wstring GetDeviceName() const override { return L"XM2W v2"; }
    const char *GetDeviceType() const override { return "XM2Wv2Mouse"; }
    int GetPriority() const override { return 2; }

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
