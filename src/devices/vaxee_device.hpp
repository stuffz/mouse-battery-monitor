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

class VaxeeDevice : public MouseDevice
{
public:
    static constexpr USHORT VID = 0x3057;
    static constexpr USHORT USAGE_PAGE = 0xFF05;
    static constexpr USHORT USAGE = 0x01;
    static constexpr BYTE REPORT_ID = 0x0E;
    static constexpr BYTE HEADER = 0xA5;
    static constexpr DWORD REPORT_SIZE = 64;

    // Command IDs
    static constexpr BYTE CMD_BATTERY_LEVEL = 0x0B;
    static constexpr BYTE CMD_CHARGING_STATUS = 0x10;
    static constexpr BYTE CMD_READ = 0x01;

    virtual ~VaxeeDevice()
    {
        Disconnect();
    }

    VaxeeDevice(const VaxeeDevice &) = delete;
    VaxeeDevice &operator=(const VaxeeDevice &) = delete;

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

        constexpr int NUM_ATTEMPTS = 2;

        try
        {
            for (int attempt = 0; attempt < NUM_ATTEMPTS; ++attempt)
            {
                LOG_DEBUG(string(GetDeviceType()) + ": Attempt " +
                          std::to_string(attempt + 1) + "/" + std::to_string(NUM_ATTEMPTS));

                // Read battery level (cmd_id 0x0B)
                if (!SendCommand(CMD_BATTERY_LEVEL, CMD_READ, 0x01))
                {
                    LOG_DEBUG(string(GetDeviceType()) + ": Failed to send battery level command");
                    continue;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                BYTE readBuffer[REPORT_SIZE] = {0};
                if (!device.GetFeatureReport(REPORT_ID, readBuffer, REPORT_SIZE))
                {
                    LOG_DEBUG(string(GetDeviceType()) + ": Failed to get battery feature report");
                    continue;
                }

                std::ostringstream oss;
                oss << GetDeviceType() << ": Response bytes [0-5]: " << std::hex << std::setfill('0')
                    << std::setw(2) << static_cast<int>(readBuffer[0]) << " "
                    << std::setw(2) << static_cast<int>(readBuffer[1]) << " "
                    << std::setw(2) << static_cast<int>(readBuffer[2]) << " "
                    << std::setw(2) << static_cast<int>(readBuffer[3]) << " "
                    << std::setw(2) << static_cast<int>(readBuffer[4]) << " "
                    << std::setw(2) << static_cast<int>(readBuffer[5]);
                LOG_DEBUG(oss.str());

                // Validate response - byte[2] should echo cmd_id
                if (readBuffer[2] == 0)
                {
                    LOG_DEBUG(string(GetDeviceType()) + ": Invalid response - no cmd_id echo");
                    continue;
                }

                int batteryLevel = (std::min)(static_cast<int>(readBuffer[5]) * 5, 100);

                // Read charging status (cmd_id 0x10)
                bool isCharging = false;
                if (SendCommand(CMD_CHARGING_STATUS, CMD_READ, 0x01))
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));

                    BYTE chargeBuffer[REPORT_SIZE] = {0};
                    if (device.GetFeatureReport(REPORT_ID, chargeBuffer, REPORT_SIZE))
                    {
                        isCharging = chargeBuffer[5] != 0;
                        LOG_DEBUG(string(GetDeviceType()) + ": Charging status byte: " +
                                  std::to_string(static_cast<int>(chargeBuffer[5])));
                    }
                }

                BatteryStatus status;
                status.percentage = batteryLevel;
                status.isCharging = isCharging;
                status.isWireless = IsDonglePID(currentPid);

                LOG_DEBUG(string(GetDeviceType()) + ": Success - Battery " +
                          std::to_string(status.percentage) + "%, Charging: " +
                          (status.isCharging ? "Yes" : "No"));
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
        return IsDonglePID(currentPid) ? L"Wireless" : L"Wired (Charging)";
    }

    USHORT GetCurrentPID() const { return currentPid; }

protected:
    VaxeeDevice() : currentPid(0) {}

    virtual vector<USHORT> GetSupportedPIDs() const = 0;
    virtual bool IsDonglePID(USHORT pid) const = 0;

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

    bool SendCommand(BYTE cmdId, BYTE readWrite, BYTE dataLength) const
    {
        BYTE writeBuffer[REPORT_SIZE] = {0};
        writeBuffer[0] = REPORT_ID;
        writeBuffer[1] = HEADER;
        writeBuffer[2] = cmdId;
        writeBuffer[3] = readWrite;
        writeBuffer[4] = dataLength;
        return device.SendFeatureReport(writeBuffer, REPORT_SIZE);
    }

    HIDDevice device;
    USHORT currentPid;
};
