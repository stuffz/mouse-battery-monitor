#pragma once

#include <string>
#include <fstream>
#include <optional>
#include <unordered_map>
#include <functional>

using std::string;

class Config
{
public:
    Config() : updateIntervalSeconds(300),
               showNotifications(true),
               lowBatteryThreshold(20),
               debugMode(false) {}

    bool Load(const string &filename)
    {
        std::ifstream file(filename);
        if (!file.is_open())
        {
            return false;
        }

        std::unordered_map<string, std::function<void(const string &)>> handlers = {
            {"update_interval_seconds", [this](const string &v)
             { updateIntervalSeconds = std::stoi(v); }},

            {"show_notifications", [this](const string &v)
             { showNotifications = ParseBool(v); }},

            {"low_battery_threshold", [this](const string &v)
             { lowBatteryThreshold = std::stoi(v); }},

            {"debug_mode", [this](const string &v)
             { debugMode = ParseBool(v); }}};

        string line;
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
    int updateIntervalSeconds;
    bool showNotifications;
    int lowBatteryThreshold;
    bool debugMode;

    struct KeyValue
    {
        string first;
        string second;
    };

    static bool ParseBool(const string &value)
    {
        return value == "true" || value == "1";
    }

    static string Trim(const string &str)
    {
        const auto start = str.find_first_not_of(" \t");
        if (start == string::npos)
        {
            return "";
        }
        const auto end = str.find_last_not_of(" \t");
        return str.substr(start, end - start + 1);
    }

    static std::optional<KeyValue> ParseLine(const string &line)
    {
        if (line.empty() || line[0] == '#' || line[0] == ';')
        {
            return std::nullopt;
        }

        size_t pos = line.find('=');
        if (pos == string::npos)
        {
            return std::nullopt;
        }

        return KeyValue{Trim(line.substr(0, pos)), Trim(line.substr(pos + 1))};
    }
};
