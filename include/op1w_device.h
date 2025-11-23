#pragma once

#include "hid_device.h"
#include "logger.h"
#include <string>
#include <thread>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <iomanip>

class OP1WDevice
{
public:
    static constexpr USHORT VID = 0x3367;
    static constexpr USHORT PID_WIRELESS = 0x1970;
    static constexpr USHORT PID_WIRED = 0x1972;
    static constexpr USHORT USAGE_PAGE = 0xFF01;
    static constexpr USHORT USAGE = 0x0002;

    struct BatteryStatus
    {
        int percentage{-1};
        bool is_charging{false};
        bool is_wireless{false};
    };

    OP1WDevice() : current_pid(0), last_status{} {}

    ~OP1WDevice()
    {
        Disconnect();
    }

    OP1WDevice(const OP1WDevice &) = delete;
    OP1WDevice &operator=(const OP1WDevice &) = delete;

    bool FindAndConnect()
    {
        return FindAndConnectWithPID(PID_WIRELESS) || FindAndConnectWithPID(PID_WIRED);
    }

    void Disconnect()
    {
        device.Close();
        current_pid = 0;
    }

    bool IsConnected() const
    {
        return device.IsOpen();
    }

    BatteryStatus ReadBattery()
    {
        if (!IsConnected())
        {
            LOG(LogLevel::Debug, "ReadBattery: Device not connected");
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
                LOG(LogLevel::Debug, "ReadBattery: Attempt " + std::to_string(attempt + 1) + "/" + std::to_string(NUM_ATTEMPTS));

                if (!SendBatteryCommand(REPORT_ID, BATTERY_CMD, REPORT_SIZE))
                {
                    LOG(LogLevel::Debug, "ReadBattery: Failed to send battery command");
                    return {};
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(350));

                BYTE read_buffer[REPORT_SIZE] = {0};
                if (!device.GetFeatureReport(REPORT_ID, read_buffer, REPORT_SIZE))
                {
                    LOG(LogLevel::Debug, "ReadBattery: Failed to get feature report");
                    return {};
                }

                std::ostringstream oss;
                oss << "ReadBattery: Response bytes [0-3]: " << std::hex << std::setfill('0')
                    << std::setw(2) << static_cast<int>(read_buffer[0]) << " "
                    << std::setw(2) << static_cast<int>(read_buffer[1]) << " "
                    << std::setw(2) << static_cast<int>(read_buffer[2]) << " "
                    << std::setw(2) << static_cast<int>(read_buffer[3])
                    << ", byte[16]: " << std::setw(2) << static_cast<int>(read_buffer[16]);
                LOG(LogLevel::Debug, oss.str());

                if (attempt == 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }

                if (read_buffer[1] != 0x01 && read_buffer[1] != 0x08)
                {
                    LOG(LogLevel::Debug, "ReadBattery: Invalid response - unexpected byte[1] value");
                    return {};
                }

                status = ParseBatteryResponse(read_buffer[16]);
                last_status = status;
                LOG(LogLevel::Debug, "ReadBattery: Success - Battery " + std::to_string(status.percentage) + "%");
                return status;
            }
        }
        catch (const std::exception &ex)
        {
            LOG(LogLevel::Error, std::string("ReadBattery exception: ") + ex.what());
        }
        catch (...)
        {
            LOG(LogLevel::Error, "ReadBattery: Unknown exception");
        }

        LOG(LogLevel::Debug, "ReadBattery: All attempts failed");
        return {};
    }

    std::wstring GetDeviceName() const { return L"OP1W"; }

    std::wstring GetConnectionMode() const
    {
        return current_pid == PID_WIRED      ? L"Wired (Charging)"
               : current_pid == PID_WIRELESS ? L"Wireless"
                                             : L"Unknown";
    }

private:
    HIDDevice device;
    USHORT current_pid;
    BatteryStatus last_status;

    bool FindAndConnectWithPID(USHORT pid)
    {
        auto devices = HIDDevice::EnumerateDevices(VID, pid);
        for (const auto &info : devices)
        {
            if (info.usage_page == USAGE_PAGE && info.usage == USAGE && device.Open(info.path))
            {
                current_pid = pid;
                return true;
            }
        }
        return false;
    }

    bool SendBatteryCommand(BYTE report_id, BYTE command, DWORD size) const
    {
        BYTE write_buffer[64] = {0};
        write_buffer[0] = report_id;
        write_buffer[1] = command;
        return device.SendFeatureReport(write_buffer, size);
    }

    BatteryStatus ParseBatteryResponse(BYTE battery_value) const
    {
        BatteryStatus status;
        status.percentage = (std::min)(static_cast<int>(battery_value), 100);
        status.is_wireless = (current_pid == PID_WIRELESS);
        status.is_charging = (current_pid == PID_WIRED);
        return status;
    }

    bool IsSupportedPID(USHORT pid) const
    {
        return pid == PID_WIRELESS || pid == PID_WIRED;
    }
};
