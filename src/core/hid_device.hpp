#pragma once

#include <windows.h>
#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <thread>
#include <chrono>

extern "C"
{
#include <hidsdi.h>
#include <setupapi.h>
#include <hidpi.h>
}

using std::vector;
using std::wstring;

struct DeviceInfo
{
    wstring path;
    USHORT vid;
    USHORT pid;
    USHORT usagePage;
    USHORT usage;
};

class HIDDevice
{
public:
    HIDDevice() : deviceHandle(INVALID_HANDLE_VALUE), vid(0), pid(0) {}

    ~HIDDevice()
    {
        Close();
    }

    HIDDevice(const HIDDevice &) = delete;
    HIDDevice &operator=(const HIDDevice &) = delete;

    static vector<DeviceInfo> EnumerateDevices(USHORT vid, USHORT pid)
    {
        vector<DeviceInfo> devices;

        GUID hidGuid;
        HidD_GetHidGuid(&hidGuid);

        HDEVINFO deviceInfoSet = SetupDiGetClassDevsW(
            &hidGuid,
            nullptr,
            nullptr,
            DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

        if (deviceInfoSet == INVALID_HANDLE_VALUE)
        {
            return devices;
        }

        SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
        deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

        for (DWORD memberIndex = 0;
             SetupDiEnumDeviceInterfaces(deviceInfoSet, nullptr, &hidGuid,
                                         memberIndex, &deviceInterfaceData);
             ++memberIndex)
        {
            if (auto deviceInfo = GetDeviceInfo(deviceInfoSet, deviceInterfaceData, vid, pid))
            {
                devices.push_back(*deviceInfo);
            }
        }

        SetupDiDestroyDeviceInfoList(deviceInfoSet);
        return devices;
    }

    bool Open(const wstring &devicePath)
    {
        Close();

        deviceHandle = CreateFileW(
            devicePath.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr);

        if (deviceHandle == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        HIDD_ATTRIBUTES attrib;
        attrib.Size = sizeof(HIDD_ATTRIBUTES);
        if (HidD_GetAttributes(deviceHandle, &attrib))
        {
            vid = attrib.VendorID;
            pid = attrib.ProductID;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        return true;
    }

    void Close()
    {
        if (deviceHandle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(deviceHandle);
            deviceHandle = INVALID_HANDLE_VALUE;
        }
    }

    bool IsOpen() const { return deviceHandle != INVALID_HANDLE_VALUE; }

    bool SendFeatureReport(const BYTE *buffer, DWORD size) const
    {
        return IsOpen() && HidD_SetFeature(deviceHandle, const_cast<BYTE *>(buffer), size) == TRUE;
    }

    bool GetFeatureReport(BYTE reportId, BYTE *buffer, DWORD size) const
    {
        if (!IsOpen())
        {
            return false;
        }

        buffer[0] = reportId;
        return HidD_GetFeature(deviceHandle, buffer, size) == TRUE;
    }

    USHORT GetVID() const { return vid; }
    USHORT GetPID() const { return pid; }

private:
    HANDLE deviceHandle;
    USHORT vid;
    USHORT pid;

    static std::optional<DeviceInfo> GetDeviceInfo(HDEVINFO deviceInfoSet,
                                                   SP_DEVICE_INTERFACE_DATA &interfaceData,
                                                   USHORT targetVid,
                                                   USHORT targetPid)
    {
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetailW(deviceInfoSet, &interfaceData, nullptr, 0, &requiredSize, nullptr);

        auto detailData = std::make_unique<BYTE[]>(requiredSize);
        auto detailPtr = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(detailData.get());
        detailPtr->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        if (!SetupDiGetDeviceInterfaceDetailW(deviceInfoSet, &interfaceData, detailPtr, requiredSize, nullptr, nullptr))
        {
            return std::nullopt;
        }

        HANDLE h = CreateFileW(detailPtr->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE)
        {
            return std::nullopt;
        }

        std::optional<DeviceInfo> result = ExtractDeviceInfo(h, detailPtr->DevicePath, targetVid, targetPid);
        CloseHandle(h);
        return result;
    }

    static std::optional<DeviceInfo> ExtractDeviceInfo(HANDLE h, const wchar_t *path, USHORT targetVid, USHORT targetPid)
    {
        HIDD_ATTRIBUTES attrib{};
        attrib.Size = sizeof(HIDD_ATTRIBUTES);
        if (!HidD_GetAttributes(h, &attrib) || attrib.VendorID != targetVid || attrib.ProductID != targetPid)
        {
            return std::nullopt;
        }

        PHIDP_PREPARSED_DATA preparsedData;
        if (!HidD_GetPreparsedData(h, &preparsedData))
        {
            return std::nullopt;
        }

        HIDP_CAPS caps;
        const bool success = (HidP_GetCaps(preparsedData, &caps) == HIDP_STATUS_SUCCESS);
        HidD_FreePreparsedData(preparsedData);

        if (!success)
        {
            return std::nullopt;
        }

        return DeviceInfo{path, attrib.VendorID, attrib.ProductID, caps.UsagePage, caps.Usage};
    }
};
