#pragma once

#include "devices/mouse_device.hpp"
#include "core/hid_device.hpp"
#include "core/logger.hpp"
#include <string>
#include <thread>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <iomanip>

using std::string;
using std::vector;
using std::wstring;

class EndgameGearDevice : public MouseDevice
{
public:
    static constexpr USHORT VID = 0x3367;
    static constexpr USHORT USAGE_PAGE = 0xFF01;
    static constexpr USHORT USAGE = 0x0002;

    virtual ~EndgameGearDevice()
    {
        Disconnect();
    }

    EndgameGearDevice(const EndgameGearDevice &) = delete;
    EndgameGearDevice &operator=(const EndgameGearDevice &) = delete;

    bool FindAndConnect() override
    {
        for (USHORT pid : GetSupportedPIDs())
        {
            if (FindAndConnectWithPID(pid))
            {
                return true;
            }
        }
        return false;
    }

    void Disconnect() override
    {
        device.Close();
        currentPid = 0;
    }

    bool IsConnected() const override
    {
        return device.IsOpen();
    }

    BatteryStatus ReadBattery() override
    {
        if (!IsConnected())
        {
            LOG_DEBUG(string(GetDeviceType()) + ": Device not connected");
            return {};
        }

        constexpr BYTE REPORT_ID = 0xA1;
        constexpr BYTE BATTERY_CMD = 0xB4;
        constexpr DWORD REPORT_SIZE = 64;
        constexpr int NUM_ATTEMPTS = 2;

        try
        {
            BatteryStatus status;

            for (int attempt = 0; attempt < NUM_ATTEMPTS; ++attempt)
            {
                LOG_DEBUG(string(GetDeviceType()) + ": Attempt " +
                          std::to_string(attempt + 1) + "/" + std::to_string(NUM_ATTEMPTS));

                if (!SendBatteryCommand(REPORT_ID, BATTERY_CMD, REPORT_SIZE))
                {
                    LOG_DEBUG(string(GetDeviceType()) + ": Failed to send battery command");
                    return {};
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(350));

                BYTE readBuffer[REPORT_SIZE] = {0};
                if (!device.GetFeatureReport(REPORT_ID, readBuffer, REPORT_SIZE))
                {
                    LOG_DEBUG(string(GetDeviceType()) + ": Failed to get feature report");
                    return {};
                }

                std::ostringstream oss;
                oss << GetDeviceType() << ": Response bytes [0-3]: " << std::hex << std::setfill('0')
                    << std::setw(2) << static_cast<int>(readBuffer[0]) << " "
                    << std::setw(2) << static_cast<int>(readBuffer[1]) << " "
                    << std::setw(2) << static_cast<int>(readBuffer[2]) << " "
                    << std::setw(2) << static_cast<int>(readBuffer[3])
                    << ", byte[16]: " << std::setw(2) << static_cast<int>(readBuffer[16]);
                LOG_DEBUG(oss.str());

                if (attempt == 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }

                if (readBuffer[1] != 0x01 && readBuffer[1] != 0x08)
                {
                    LOG_DEBUG(string(GetDeviceType()) + ": Invalid response - unexpected byte[1] value");
                    return {};
                }

                status = ParseBatteryResponse(readBuffer[16]);
                lastStatus = status;
                LOG_DEBUG(string(GetDeviceType()) + ": Success - Battery " +
                          std::to_string(status.percentage) + "%");
                return status;
            }
        }
        catch (const std::exception &ex)
        {
            LOG_ERROR(string(GetDeviceType()) + " exception: " + ex.what());
        }
        catch (...)
        {
            LOG_ERROR(string(GetDeviceType()) + ": Unknown exception");
        }

        LOG_DEBUG(string(GetDeviceType()) + ": All attempts failed");
        return {};
    }

    virtual wstring GetDeviceName() const = 0;
    virtual const char *GetDeviceType() const = 0;
    virtual int GetPriority() const = 0;

    wstring GetConnectionMode() const override
    {
        if (currentPid == 0)
            return L"Unknown";
        return IsWiredPID(currentPid) ? L"Wired (Charging)" : L"Wireless";
    }

    USHORT GetCurrentPID() const { return currentPid; }

protected:
    EndgameGearDevice() : currentPid(0), lastStatus{} {}

    virtual vector<USHORT> GetSupportedPIDs() const = 0;
    virtual bool IsWiredPID(USHORT pid) const = 0;

    bool FindAndConnectWithPID(USHORT pid)
    {
        auto devices = HIDDevice::EnumerateDevices(VID, pid);
        for (const auto &info : devices)
        {
            if (info.usagePage == USAGE_PAGE && info.usage == USAGE && device.Open(info.path))
            {
                currentPid = pid;
                std::ostringstream pidStream;
                pidStream << std::hex << std::uppercase << pid;
                LOG_INFO(string(GetDeviceType()) + " connected (PID: 0x" +
                         pidStream.str() + ")");
                return true;
            }
        }
        return false;
    }

    bool SendBatteryCommand(BYTE reportId, BYTE command, DWORD size) const
    {
        BYTE writeBuffer[64] = {0};
        writeBuffer[0] = reportId;
        writeBuffer[1] = command;
        return device.SendFeatureReport(writeBuffer, size);
    }

    BatteryStatus ParseBatteryResponse(BYTE batteryValue) const
    {
        BatteryStatus status;
        status.percentage = (std::min)(static_cast<int>(batteryValue), 100);
        status.isWireless = !IsWiredPID(currentPid);
        status.isCharging = IsWiredPID(currentPid);
        return status;
    }

    HIDDevice device;
    USHORT currentPid;
    BatteryStatus lastStatus;
};
