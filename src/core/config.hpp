#pragma once

#include <charconv>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

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

        const std::unordered_map<std::string, std::function<bool(const std::string &)>> handlers = {
            {"update_interval_seconds",
             [this](const std::string &v) { return SetInt(v, updateIntervalSeconds); }},

            {"show_notifications",
             [this](const std::string &v)
             {
                 showNotifications = ParseBool(v);
                 return true;
             }},

            {"low_battery_threshold",
             [this](const std::string &v) { return SetInt(v, lowBatteryThreshold); }},

            {"debug_mode", [this](const std::string &v)
             {
                 debugMode = ParseBool(v);
                 return true;
             }}};

        std::string line;
        while (std::getline(file, line))
        {
            if (auto kv = ParseLine(line))
            {
                auto it = handlers.find(kv->first);
                if (it != handlers.end() && !it->second(kv->second))
                {
                    errors.push_back(kv->first + "=" + kv->second +
                                     " is not a valid value, keeping the default");
                }
            }
        }

        return true;
    }

    int GetUpdateIntervalSeconds() const { return updateIntervalSeconds; }
    bool GetShowNotifications() const { return showNotifications; }
    int GetLowBatteryThreshold() const { return lowBatteryThreshold; }
    bool GetDebugMode() const { return debugMode; }

    // Load runs before the logger is set up, so the caller logs these.
    const std::vector<std::string> &GetErrors() const { return errors; }

private:
    int updateIntervalSeconds = 300;
    bool showNotifications = true;
    int lowBatteryThreshold = 20;
    bool debugMode = false;
    std::vector<std::string> errors;

    struct KeyValue
    {
        std::string first;
        std::string second;
    };

    static bool ParseBool(const std::string &value) { return value == "true" || value == "1"; }

    // Not std::stoi: it throws on "five", which ends the app before the tray
    // starts, and reads "5m" as 5.
    static bool SetInt(const std::string &text, int &target)
    {
        int value = 0;
        const char *end = text.data() + text.size();
        const auto [parsedEnd, error] = std::from_chars(text.data(), end, value);

        if (error != std::errc() || parsedEnd != end)
        {
            return false;
        }

        target = value;
        return true;
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

        const auto pos = line.find('=');
        if (pos == std::string::npos)
        {
            return std::nullopt;
        }

        return KeyValue{Trim(line.substr(0, pos)), Trim(line.substr(pos + 1))};
    }
};
