#pragma once

#include "core/hid_device.hpp"
#include "core/logger.hpp"
#include "devices/mouse_device.hpp"
#include "platform/platform.hpp"
#include <algorithm>
#include <chrono>
#include <exception>
#include <iomanip>
#include <ios>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

class EndgameGearDevice : public MouseDevice
{
public:
    static constexpr USHORT VID = 0x3367;
    static constexpr USHORT USAGE_PAGE = 0xFF01;
    static constexpr USHORT USAGE = 0x0002;

    // Qualified: virtual dispatch is already off in a destructor.
    ~EndgameGearDevice() override { EndgameGearDevice::Disconnect(); }

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

    bool IsConnected() const override { return device.IsOpen(); }

    BatteryStatus ReadBattery() override
    {
        if (!IsConnected())
        {
            LOG_DEBUG(std::string(GetDeviceType()) + ": Device not connected");
            return {};
        }

        constexpr BYTE REPORT_ID = 0xA1;
        constexpr BYTE BATTERY_CMD = 0xB4;
        constexpr BYTE STATUS_OK = 0x01;
        constexpr BYTE STATUS_MOUSE_UNREACHABLE = 0x08;
        constexpr int NUM_ATTEMPTS = 2;

        try
        {
            BatteryStatus status;

            for (int attempt = 0; attempt < NUM_ATTEMPTS; ++attempt)
            {
                LOG_DEBUG(std::string(GetDeviceType()) + ": Attempt " +
                          std::to_string(attempt + 1) + "/" + std::to_string(NUM_ATTEMPTS));

                if (!SendBatteryCommand(REPORT_ID, BATTERY_CMD, REPORT_SIZE))
                {
                    LOG_DEBUG(std::string(GetDeviceType()) + ": Failed to send battery command");
                    return {};
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(350));

                BYTE readBuffer[REPORT_SIZE] = {0};
                if (!device.GetFeatureReport(REPORT_ID, readBuffer, REPORT_SIZE))
                {
                    LOG_DEBUG(std::string(GetDeviceType()) + ": Failed to get feature report");
                    return {};
                }

                LOG_DEBUG(DescribeResponse(readBuffer, REPORT_SIZE));

                if (attempt == 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }

                if (readBuffer[STATUS_OFFSET] == STATUS_MOUSE_UNREACHABLE)
                {
                    // The dongle answers 0x08 when the mouse is asleep or switched
                    // off. The payload is then the previous reply still sitting in
                    // its buffer, so byte[16] is stale and must not be reported.
                    LOG_DEBUG(std::string(GetDeviceType()) +
                              ": Mouse unreachable (asleep or off) - stale payload ignored");
                    return {};
                }

                if (readBuffer[STATUS_OFFSET] != STATUS_OK)
                {
                    LOG_DEBUG(std::string(GetDeviceType()) +
                              ": Invalid response - unexpected status byte");
                    return {};
                }

                status = ParseBatteryResponse(readBuffer[PERCENT_OFFSET]);
                LOG_DEBUG(std::string(GetDeviceType()) + ": Success - Battery " +
                          std::to_string(status.percentage) + "%");
                return status;
            }
        }
        catch (const std::exception &ex)
        {
            LOG_ERROR(std::string(GetDeviceType()) + " exception: " + ex.what());
        }
        catch (...)
        {
            LOG_ERROR(std::string(GetDeviceType()) + ": Unknown exception");
        }

        LOG_DEBUG(std::string(GetDeviceType()) + ": All attempts failed");
        return {};
    }

    std::wstring GetConnectionMode() const override
    {
        if (currentPid == 0)
            return L"Unknown";
        return IsWiredPID(currentPid) ? L"Wired (Charging)" : L"Wireless";
    }

protected:
    EndgameGearDevice() = default;

    virtual std::vector<USHORT> GetSupportedPIDs() const = 0;
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
                LOG_INFO(std::string(GetDeviceType()) + " connected (PID: 0x" + pidStream.str() +
                         ")");
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
        status.isCharging = IsWiredPID(currentPid);
        return status;
    }

    // The whole reply goes to the log so a user's debug output can answer
    // protocol questions nobody thought to ask yet; bytes past the documented
    // fields are where the dongle's stale-buffer behaviour first showed up.
    std::string DescribeResponse(const BYTE *buffer, DWORD size) const
    {
        std::ostringstream oss;
        oss << GetDeviceType() << ": Response:" << std::hex << std::setfill('0');

        for (DWORD i = 0; i < size; ++i)
        {
            oss << ' ' << std::setw(2) << static_cast<int>(buffer[i]);
        }

        oss << " | status=0x" << std::setw(2) << static_cast<int>(buffer[STATUS_OFFSET]) << std::dec
            << " percent=" << static_cast<int>(buffer[PERCENT_OFFSET])
            << " voltage=" << (buffer[VOLTAGE_OFFSET] | (buffer[VOLTAGE_OFFSET + 1] << 8)) << "mV";
        return oss.str();
    }

    static constexpr DWORD REPORT_SIZE = 64;
    static constexpr DWORD STATUS_OFFSET = 1;
    static constexpr DWORD PERCENT_OFFSET = 16;
    static constexpr DWORD VOLTAGE_OFFSET = 17; // uint16 little endian, millivolts

    HIDDevice device;
    USHORT currentPid = 0;
};
