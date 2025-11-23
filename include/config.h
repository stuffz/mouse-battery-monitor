#pragma once

#include <string>
#include <fstream>
#include <optional>
#include <unordered_map>
#include <functional>

class Config
{
public:
    Config() : update_interval_seconds(10),
               show_notifications(true),
               low_battery_threshold(20),
               debug_mode(false) {}

    bool Load(const std::string &filename)
    {
        std::ifstream file(filename);
        if (!file.is_open())
        {
            return false;
        }

        std::unordered_map<std::string, std::function<void(const std::string &)>> handlers = {
            {"update_interval_seconds", [this](const std::string &v)
             { update_interval_seconds = std::stoi(v); }},

            {"show_notifications", [this](const std::string &v)
             { show_notifications = ParseBool(v); }},

            {"low_battery_threshold", [this](const std::string &v)
             { low_battery_threshold = std::stoi(v); }},

            {"debug_mode", [this](const std::string &v)
             { debug_mode = ParseBool(v); }}};

        std::string line;
        while (std::getline(file, line))
        {
            if (auto kv = ParseLine(line))
            {
                auto it = handlers.find(kv->first);
                if (it != handlers.end())
                {
                    it->second(kv->second);
                }
            }
        }

        return true;
    }

    int GetUpdateIntervalSeconds() const { return update_interval_seconds; }
    bool GetShowNotifications() const { return show_notifications; }
    int GetLowBatteryThreshold() const { return low_battery_threshold; }
    bool GetDebugMode() const { return debug_mode; }

private:
    int update_interval_seconds;
    bool show_notifications;
    int low_battery_threshold;
    bool debug_mode;

    struct KeyValue
    {
        std::string first;
        std::string second;
    };

    static bool ParseBool(const std::string &value)
    {
        return value == "true" || value == "1";
    }

    static std::string Trim(const std::string &str)
    {
        const auto start = str.find_first_not_of(" \t");
        if (start == std::string::npos)
        {
            return "";
        }
        const auto end = str.find_last_not_of(" \t");
        return str.substr(start, end - start + 1);
    }

    static std::optional<KeyValue> ParseLine(const std::string &line)
    {
        if (line.empty() || line[0] == '#' || line[0] == ';')
        {
            return std::nullopt;
        }

        size_t pos = line.find('=');
        if (pos == std::string::npos)
        {
            return std::nullopt;
        }

        return KeyValue{Trim(line.substr(0, pos)), Trim(line.substr(pos + 1))};
    }
};
