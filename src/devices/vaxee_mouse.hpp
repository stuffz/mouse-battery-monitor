#pragma once

#include "devices/vaxee_device.hpp"

class VaxeeMouse : public VaxeeDevice
{
public:
    std::wstring GetDeviceName() const override
    {
        return GetNameForPID(currentPid);
    }

    const char *GetDeviceType() const override { return "VaxeeMouse"; }
    int GetPriority() const override { return 4; }

protected:
    std::vector<USHORT> GetSupportedPIDs() const override
    {
        return {
            0x1003, // VAXEE XE Wireless
            0x1004, // ZYGEN NP-01S Wireless
            0x1005, // VAXEE AX Wireless
            0x1006, // ZYGEN NP-01 Wireless
            0x1007, // VAXEE XE-S Wireless
            0x1008, // VAXEE XE-S-L Wireless
            0x1009, // VAXEE x NINJUTSO Sora Wireless
            0x1010, // VAXEE E1 Wireless
            0x1011, // ZYGEN NP-01S V2 Wireless
            0x1012, // VAXEE XE V2 Wireless
            0x1013, // ZYGEN NP-01S Ergo Wireless
        };
    }

    bool IsDonglePID(USHORT) const override
    {
        return false;
    }

private:
    static std::wstring GetNameForPID(USHORT pid)
    {
        switch (pid)
        {
        case 0x1003:
            return L"VAXEE XE Wireless";
        case 0x1004:
            return L"ZYGEN NP-01S Wireless";
        case 0x1005:
            return L"VAXEE AX Wireless";
        case 0x1006:
            return L"ZYGEN NP-01 Wireless";
        case 0x1007:
            return L"VAXEE XE-S Wireless";
        case 0x1008:
            return L"VAXEE XE-S-L Wireless";
        case 0x1009:
            return L"VAXEE x NINJUTSO Sora Wireless";
        case 0x1010:
            return L"VAXEE E1 Wireless";
        case 0x1011:
            return L"ZYGEN NP-01S V2 Wireless";
        case 0x1012:
            return L"VAXEE XE V2 Wireless";
        case 0x1013:
            return L"ZYGEN NP-01S Ergo Wireless";
        default:
            return L"VAXEE Mouse";
        }
    }
};
