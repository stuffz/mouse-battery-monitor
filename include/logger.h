#pragma once

#include <string>
#include <string_view>
#include <fstream>
#include <mutex>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <array>

enum class LogLevel
{
    Info,
    Debug,
    Error
};

class Logger
{
public:
    static Logger &Instance()
    {
        static Logger instance;
        return instance;
    }

    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    void SetDebugMode(bool enabled)
    {
        debug_mode = enabled;
    }

    void SetLogFile(const std::string &filename)
    {
        std::lock_guard<std::mutex> lock(log_mutex);

        if (log_file.is_open())
        {
            log_file.close();
        }

        log_filename = filename;
        log_file.open(log_filename, std::ios::app);
    }

    void Log(LogLevel level, const std::string &message)
    {
        if (ShouldLog(level))
        {
            WriteToFile(level, message);
        }

        if (debug_mode)
        {
            WriteToConsole(level, message);
        }
    }

private:
    Logger() : debug_mode(false), log_filename("battery_monitor.log") {}

    ~Logger()
    {
        std::lock_guard<std::mutex> lock(log_mutex);
        if (log_file.is_open())
        {
            log_file.close();
        }
    }

    bool ShouldLog(LogLevel level) const
    {
        return level != LogLevel::Debug || debug_mode;
    }

    void WriteToFile(LogLevel level, const std::string &message)
    {
        std::lock_guard<std::mutex> lock(log_mutex);

        if (!log_file.is_open())
        {
            log_file.open(log_filename, std::ios::app);
        }

        if (log_file.is_open())
        {
            log_file << "[" << GetTimestamp() << "] [" << LevelToString(level)
                     << "] " << message << std::endl;
            log_file.flush();
        }
    }

    void WriteToConsole(LogLevel level, const std::string &message) const
    {
        std::cout << "[" << LevelToString(level) << "] " << message << std::endl;
    }

    static std::string GetTimestamp()
    {
        const auto now = std::chrono::system_clock::now();
        const auto time_t = std::chrono::system_clock::to_time_t(now);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now.time_since_epoch()) %
                        1000;

        std::tm tm;
        localtime_s(&tm, &time_t);

        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
    }

    static constexpr std::string_view LevelToString(LogLevel level)
    {
        constexpr std::array<std::string_view, 3> level_names = {"INFO", "DEBUG", "ERROR"};
        return level_names[static_cast<size_t>(level)];
    }

    bool debug_mode;
    std::string log_filename;
    std::ofstream log_file;
    std::mutex log_mutex;
};

#define LOG(level, msg) Logger::Instance().Log(level, msg)
