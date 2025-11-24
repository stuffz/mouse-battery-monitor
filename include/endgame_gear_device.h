#pragma once

#include "hid_device.h"
#include "logger.h"
#include <string>
#include <thread>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <iomanip>

class EndgameGearDevice
{
public:
    static constexpr USHORT VID = 0x3367;
    static constexpr USHORT USAGE_PAGE = 0xFF01;
    static constexpr USHORT USAGE = 0x0002;

    struct BatteryStatus
    {
        int percentage{-1};
        bool is_charging{false};
        bool is_wireless{false};
    };

    virtual ~EndgameGearDevice()
    {
        Disconnect();
    }

    EndgameGearDevice(const EndgameGearDevice &) = delete;
    EndgameGearDevice &operator=(const EndgameGearDevice &) = delete;

    bool FindAndConnect()
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
            LOG(LogLevel::Debug, std::string(GetDeviceType()) + ": Device not connected");
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
                LOG(LogLevel::Debug, std::string(GetDeviceType()) + ": Attempt " +
                                         std::to_string(attempt + 1) + "/" + std::to_string(NUM_ATTEMPTS));

                if (!SendBatteryCommand(REPORT_ID, BATTERY_CMD, REPORT_SIZE))
                {
                    LOG(LogLevel::Debug, std::string(GetDeviceType()) + ": Failed to send battery command");
                    return {};
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(350));

                BYTE read_buffer[REPORT_SIZE] = {0};
                if (!device.GetFeatureReport(REPORT_ID, read_buffer, REPORT_SIZE))
                {
                    LOG(LogLevel::Debug, std::string(GetDeviceType()) + ": Failed to get feature report");
                    return {};
                }

                std::ostringstream oss;
                oss << GetDeviceType() << ": Response bytes [0-3]: " << std::hex << std::setfill('0')
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
                    LOG(LogLevel::Debug, std::string(GetDeviceType()) + ": Invalid response - unexpected byte[1] value");
                    return {};
                }

                status = ParseBatteryResponse(read_buffer[16]);
                last_status = status;
                LOG(LogLevel::Debug, std::string(GetDeviceType()) + ": Success - Battery " +
                                         std::to_string(status.percentage) + "%");
                return status;
            }
        }
        catch (const std::exception &ex)
        {
            LOG(LogLevel::Error, std::string(GetDeviceType()) + " exception: " + ex.what());
        }
        catch (...)
        {
            LOG(LogLevel::Error, std::string(GetDeviceType()) + ": Unknown exception");
        }

        LOG(LogLevel::Debug, std::string(GetDeviceType()) + ": All attempts failed");
        return {};
    }

    virtual std::wstring GetDeviceName() const = 0;
    virtual const char *GetDeviceType() const = 0;
    virtual int GetPriority() const = 0;

    std::wstring GetConnectionMode() const
    {
        if (current_pid == 0)
            return L"Unknown";
        return IsWiredPID(current_pid) ? L"Wired (Charging)" : L"Wireless";
    }

    USHORT GetCurrentPID() const { return current_pid; }

protected:
    EndgameGearDevice() : current_pid(0), last_status{} {}

    virtual std::vector<USHORT> GetSupportedPIDs() const = 0;
    virtual bool IsWiredPID(USHORT pid) const = 0;

    bool FindAndConnectWithPID(USHORT pid)
    {
        auto devices = HIDDevice::EnumerateDevices(VID, pid);
        for (const auto &info : devices)
        {
            if (info.usage_page == USAGE_PAGE && info.usage == USAGE && device.Open(info.path))
            {
                current_pid = pid;
                LOG(LogLevel::Info, std::string(GetDeviceType()) + " connected (PID: 0x" +
                                        std::to_string(pid) + ")");
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
        status.is_wireless = !IsWiredPID(current_pid);
        status.is_charging = IsWiredPID(current_pid);
        return status;
    }

    HIDDevice device;
    USHORT current_pid;
    BatteryStatus last_status;
};
