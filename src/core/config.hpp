#pragma once

#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

class Config
{
public:
    Config() = default;

    bool Load(const std::string &filename)
    {
        std::ifstream file(filename);
        if (!file.is_open())
        {
            return false;
        }

        const std::unordered_map<std::string, std::function<void(const std::string &)>> handlers = {
            {"update_interval_seconds",
             [this](const std::string &v) { updateIntervalSeconds = std::stoi(v); }},

            {"show_notifications",
             [this](const std::string &v) { showNotifications = ParseBool(v); }},

            {"low_battery_threshold",
             [this](const std::string &v) { lowBatteryThreshold = std::stoi(v); }},

            {"debug_mode", [this](const std::string &v) { debugMode = ParseBool(v); }}};

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

    int GetUpdateIntervalSeconds() const { return updateIntervalSeconds; }
    bool GetShowNotifications() const { return showNotifications; }
    int GetLowBatteryThreshold() const { return lowBatteryThreshold; }
    bool GetDebugMode() const { return debugMode; }

private:
    int updateIntervalSeconds = 300;
    bool showNotifications = true;
    int lowBatteryThreshold = 20;
    bool debugMode = false;

    struct KeyValue
    {
        std::string first;
        std::string second;
    };

    static bool ParseBool(const std::string &value) { return value == "true" || value == "1"; }

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

        const auto pos = line.find('=');
        if (pos == std::string::npos)
        {
            return std::nullopt;
        }

        return KeyValue{Trim(line.substr(0, pos)), Trim(line.substr(pos + 1))};
    }
};
