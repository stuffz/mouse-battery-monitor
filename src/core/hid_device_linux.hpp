#pragma once

#include "core/device_info.hpp"
#include "core/logger.hpp"
#include "platform/platform.hpp"

#include <fcntl.h>
#include <linux/hidraw.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

// The feature-report ioctls take the report ID in byte 0, exactly like the
// Windows HidD_SetFeature / HidD_GetFeature pair.
class HIDDevice
{
public:
    HIDDevice() = default;

    ~HIDDevice() { Close(); }

    HIDDevice(const HIDDevice &) = delete;
    HIDDevice &operator=(const HIDDevice &) = delete;

    static std::vector<DeviceInfo> EnumerateDevices(USHORT vid, USHORT pid)
    {
        namespace fs = std::filesystem;

        std::vector<DeviceInfo> devices;
        std::error_code ec;

        fs::directory_iterator it("/sys/class/hidraw", ec);
        if (ec)
        {
            LOG_ERROR("Failed to scan /sys/class/hidraw: " + ec.message());
            return devices;
        }

        // Stepped by hand: the range-for uses the throwing increment, and a node
        // vanishing mid-scan is routine here rather than exceptional.
        for (const fs::directory_iterator end; it != end; it.increment(ec))
        {
            if (ec)
            {
                LOG_DEBUG("Stopped scanning /sys/class/hidraw: " + ec.message());
                break;
            }

            const fs::path entry = it->path();
            const fs::path hidPath = entry / "device";

            const auto ids = ReadHidIds(hidPath / "uevent");
            if (!ids || ids->vid != vid || ids->pid != pid)
            {
                continue;
            }

            const auto descriptor = ReadBinaryFile(hidPath / "report_descriptor");
            if (descriptor.empty())
            {
                continue;
            }

            const std::string node = "/dev/" + entry.filename().string();
            for (const auto &collection : ParseTopLevelCollections(descriptor))
            {
                devices.push_back(DeviceInfo{node, collection.usagePage, collection.usage});
            }
        }

        return devices;
    }

    bool Open(const NativePath &devicePath)
    {
        Close();

        fd = ::open(devicePath.c_str(), O_RDWR | O_CLOEXEC);

        if (fd < 0)
        {
            LOG_DEBUG("Failed to open " + devicePath + ": " + std::strerror(errno));
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        return true;
    }

    void Close()
    {
        if (fd >= 0)
        {
            ::close(fd);
            fd = -1;
        }
    }

    bool IsOpen() const { return fd >= 0; }

    bool SendFeatureReport(const BYTE *buffer, DWORD size) const
    {
        if (!IsOpen())
        {
            return false;
        }

        // The ioctl needs a writable buffer, so work on a copy.
        std::vector<BYTE> report(buffer, buffer + size);

        if (::ioctl(fd, HIDIOCSFEATURE(size), report.data()) < 0)
        {
            LOG_DEBUG(std::string("HIDIOCSFEATURE failed: ") + std::strerror(errno));
            return false;
        }

        return true;
    }

    bool GetFeatureReport(BYTE reportId, BYTE *buffer, DWORD size) const
    {
        if (!IsOpen())
        {
            return false;
        }

        buffer[0] = reportId;

        if (::ioctl(fd, HIDIOCGFEATURE(size), buffer) < 0)
        {
            LOG_DEBUG(std::string("HIDIOCGFEATURE failed: ") + std::strerror(errno));
            return false;
        }

        return true;
    }

private:
    int fd = -1;

    struct HidIds
    {
        USHORT vid;
        USHORT pid;
    };

    struct TopLevelCollection
    {
        USHORT usagePage;
        USHORT usage;
    };

    // HID item prefix decoding (Device Class Definition for HID 1.11, §6.2.2).
    static constexpr BYTE TYPE_MAIN = 0;
    static constexpr BYTE TYPE_GLOBAL = 1;
    static constexpr BYTE TYPE_LOCAL = 2;
    static constexpr BYTE TAG_USAGE = 0x0;
    static constexpr BYTE TAG_USAGE_PAGE = 0x0;
    static constexpr BYTE TAG_COLLECTION = 0xA;
    static constexpr BYTE TAG_END_COLLECTION = 0xC;
    static constexpr BYTE TAG_PUSH = 0xA;
    static constexpr BYTE TAG_POP = 0xB;
    static constexpr BYTE LONG_ITEM_PREFIX = 0xFE;
    static constexpr BYTE COLLECTION_APPLICATION = 0x01;

    // sysfs reports HID_ID as "bus:vendor:product" with hex fields.
    static std::optional<HidIds> ReadHidIds(const std::filesystem::path &ueventPath)
    {
        std::ifstream file(ueventPath);
        if (!file.is_open())
        {
            return std::nullopt;
        }

        std::string line;
        while (std::getline(file, line))
        {
            unsigned bus = 0, vid = 0, pid = 0;
            // NOLINTNEXTLINE(cert-err34-c,bugprone-unchecked-string-to-number-conversion)
            if (std::sscanf(line.c_str(), "HID_ID=%x:%x:%x", &bus, &vid, &pid) == 3)
            {
                return HidIds{static_cast<USHORT>(vid), static_cast<USHORT>(pid)};
            }
        }

        return std::nullopt;
    }

    static std::vector<BYTE> ReadBinaryFile(const std::filesystem::path &path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
        {
            return {};
        }

        return std::vector<BYTE>(std::istreambuf_iterator<char>(file),
                                 std::istreambuf_iterator<char>());
    }

    static std::vector<TopLevelCollection>
    ParseTopLevelCollections(const std::vector<BYTE> &descriptor)
    {
        std::vector<TopLevelCollection> collections;

        USHORT usagePage = 0;
        USHORT pendingUsagePage = 0;
        USHORT pendingUsage = 0;
        bool haveUsage = false;
        int depth = 0;
        std::vector<USHORT> pageStack;

        for (size_t i = 0; i < descriptor.size();)
        {
            const BYTE prefix = descriptor[i];

            if (prefix == LONG_ITEM_PREFIX)
            {
                if (i + 1 >= descriptor.size())
                {
                    break;
                }
                i += 3 + descriptor[i + 1];
                continue;
            }

            const BYTE tag = static_cast<BYTE>(prefix >> 4);
            const BYTE type = static_cast<BYTE>((prefix >> 2) & 0x03);
            const size_t dataSize = (prefix & 0x03) == 3 ? 4 : (prefix & 0x03);

            if (i + 1 + dataSize > descriptor.size())
            {
                break;
            }

            std::uint32_t value = 0;
            for (size_t b = 0; b < dataSize; ++b)
            {
                value |= static_cast<std::uint32_t>(descriptor[i + 1 + b]) << (8 * b);
            }

            if (type == TYPE_GLOBAL && tag == TAG_USAGE_PAGE)
            {
                usagePage = static_cast<USHORT>(value);
            }
            else if (type == TYPE_GLOBAL && tag == TAG_PUSH)
            {
                pageStack.push_back(usagePage);
            }
            else if (type == TYPE_GLOBAL && tag == TAG_POP)
            {
                if (!pageStack.empty())
                {
                    usagePage = pageStack.back();
                    pageStack.pop_back();
                }
            }
            else if (type == TYPE_LOCAL && tag == TAG_USAGE)
            {
                // An extended usage carries its page for this usage only; being a
                // local item it must not overwrite the global usage page.
                pendingUsagePage = (dataSize == 4) ? static_cast<USHORT>(value >> 16) : usagePage;
                pendingUsage = static_cast<USHORT>(value & 0xFFFF);
                haveUsage = true;
            }
            else if (type == TYPE_MAIN)
            {
                if (tag == TAG_COLLECTION)
                {
                    if (depth == 0 && value == COLLECTION_APPLICATION && haveUsage)
                    {
                        collections.push_back({pendingUsagePage, pendingUsage});
                    }
                    ++depth;
                }
                else if (tag == TAG_END_COLLECTION && depth > 0)
                {
                    --depth;
                }

                // Every main item clears the local item state.
                haveUsage = false;
            }

            i += 1 + dataSize;
        }

        return collections;
    }
};
