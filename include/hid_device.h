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

struct DeviceInfo
{
    std::wstring path;
    USHORT vid;
    USHORT pid;
    USHORT usage_page;
    USHORT usage;
};

class HIDDevice
{
public:
    HIDDevice() : device_handle(INVALID_HANDLE_VALUE), vid(0), pid(0) {}

    ~HIDDevice()
    {
        Close();
    }

    HIDDevice(const HIDDevice &) = delete;
    HIDDevice &operator=(const HIDDevice &) = delete;

    static std::vector<DeviceInfo> EnumerateDevices(USHORT vid, USHORT pid)
    {
        std::vector<DeviceInfo> devices;

        GUID hid_guid;
        HidD_GetHidGuid(&hid_guid);

        HDEVINFO device_info_set = SetupDiGetClassDevsW(
            &hid_guid,
            nullptr,
            nullptr,
            DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

        if (device_info_set == INVALID_HANDLE_VALUE)
        {
            return devices;
        }

        SP_DEVICE_INTERFACE_DATA device_interface_data;
        device_interface_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

        for (DWORD member_index = 0;
             SetupDiEnumDeviceInterfaces(device_info_set, nullptr, &hid_guid,
                                         member_index, &device_interface_data);
             ++member_index)
        {
            if (auto device_info = GetDeviceInfo(device_info_set, device_interface_data, vid, pid))
            {
                devices.push_back(*device_info);
            }
        }

        SetupDiDestroyDeviceInfoList(device_info_set);
        return devices;
    }

    bool Open(const std::wstring &device_path)
    {
        Close();

        device_handle = CreateFileW(
            device_path.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr);

        if (device_handle == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        // Get device attributes
        HIDD_ATTRIBUTES attrib;
        attrib.Size = sizeof(HIDD_ATTRIBUTES);
        if (HidD_GetAttributes(device_handle, &attrib))
        {
            vid = attrib.VendorID;
            pid = attrib.ProductID;
        }

        // Small delay for device initialization
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        return true;
    }

    void Close()
    {
        if (device_handle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(device_handle);
            device_handle = INVALID_HANDLE_VALUE;
        }
    }

    bool IsOpen() const { return device_handle != INVALID_HANDLE_VALUE; }

    // HID communication
    bool SendFeatureReport(const BYTE *buffer, DWORD size) const
    {
        return IsOpen() && HidD_SetFeature(device_handle, const_cast<BYTE *>(buffer), size) == TRUE;
    }

    bool GetFeatureReport(BYTE report_id, BYTE *buffer, DWORD size) const
    {
        if (!IsOpen())
        {
            return false;
        }

        buffer[0] = report_id;
        return HidD_GetFeature(device_handle, buffer, size) == TRUE;
    }

    USHORT GetVID() const { return vid; }
    USHORT GetPID() const { return pid; }

private:
    HANDLE device_handle;
    USHORT vid;
    USHORT pid;

    static std::optional<DeviceInfo> GetDeviceInfo(HDEVINFO device_info_set,
                                                   SP_DEVICE_INTERFACE_DATA &interface_data,
                                                   USHORT target_vid,
                                                   USHORT target_pid)
    {
        DWORD required_size = 0;
        SetupDiGetDeviceInterfaceDetailW(device_info_set, &interface_data, nullptr, 0, &required_size, nullptr);

        auto detail_data = std::make_unique<BYTE[]>(required_size);
        auto detail_ptr = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(detail_data.get());
        detail_ptr->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        if (!SetupDiGetDeviceInterfaceDetailW(device_info_set, &interface_data, detail_ptr, required_size, nullptr, nullptr))
        {
            return std::nullopt;
        }

        HANDLE h = CreateFileW(detail_ptr->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE)
        {
            return std::nullopt;
        }

        std::optional<DeviceInfo> result = ExtractDeviceInfo(h, detail_ptr->DevicePath, target_vid, target_pid);
        CloseHandle(h);
        return result;
    }

    static std::optional<DeviceInfo> ExtractDeviceInfo(HANDLE h, const wchar_t *path, USHORT target_vid, USHORT target_pid)
    {
        HIDD_ATTRIBUTES attrib{};
        attrib.Size = sizeof(HIDD_ATTRIBUTES);
        if (!HidD_GetAttributes(h, &attrib) || attrib.VendorID != target_vid || attrib.ProductID != target_pid)
        {
            return std::nullopt;
        }

        PHIDP_PREPARSED_DATA preparsed_data;
        if (!HidD_GetPreparsedData(h, &preparsed_data))
        {
            return std::nullopt;
        }

        HIDP_CAPS caps;
        const bool success = (HidP_GetCaps(preparsed_data, &caps) == HIDP_STATUS_SUCCESS);
        HidD_FreePreparsedData(preparsed_data);

        if (!success)
        {
            return std::nullopt;
        }

        return DeviceInfo{path, attrib.VendorID, attrib.ProductID, caps.UsagePage, caps.Usage};
    }
};
