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

using std::chrono::system_clock;

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
        debugMode = enabled;
    }

    void SetLogFile(const std::string &filename)
    {
        std::lock_guard<std::mutex> lock(logMutex);

        if (logFile.is_open())
        {
            logFile.close();
        }

        logFilename = filename;
        logFile.open(logFilename, std::ios::app);
    }

    void Log(LogLevel level, const std::string &message)
    {
        if (ShouldLog(level))
        {
            WriteToFile(level, message);
        }

        if (debugMode)
        {
            WriteToConsole(level, message);
        }
    }

private:
    Logger() : debugMode(false), logFilename("battery_monitor.log") {}

    ~Logger()
    {
        std::lock_guard<std::mutex> lock(logMutex);
        if (logFile.is_open())
        {
            logFile.close();
        }
    }

    bool ShouldLog(LogLevel level) const
    {
        return level != LogLevel::Debug || debugMode;
    }

    void WriteToFile(LogLevel level, const std::string &message)
    {
        std::lock_guard<std::mutex> lock(logMutex);

        if (!logFile.is_open())
        {
            logFile.open(logFilename, std::ios::app);
        }

        if (logFile.is_open())
        {
            logFile << "[" << GetTimestamp() << "] [" << LevelToString(level)
                    << "] " << message << std::endl;
            logFile.flush();
        }
    }

    void WriteToConsole(LogLevel level, const std::string &message) const
    {
        std::cout << "[" << LevelToString(level) << "] " << message << std::endl;
    }

    static std::string GetTimestamp()
    {
        const auto now = std::chrono::system_clock::now();
        const auto timeT = std::chrono::system_clock::to_time_t(now);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now.time_since_epoch()) %
                        1000;

        std::tm tm;
        localtime_s(&tm, &timeT);

        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
    }

    static constexpr std::string_view LevelToString(LogLevel level)
    {
        constexpr std::array<std::string_view, 3> levelNames = {"INFO", "DEBUG", "ERROR"};
        return levelNames[static_cast<size_t>(level)];
    }

    bool debugMode;
    std::string logFilename;
    std::ofstream logFile;
    std::mutex logMutex;
};

#define LOG_INFO(msg) Logger::Instance().Log(LogLevel::Info, msg)
#define LOG_DEBUG(msg) Logger::Instance().Log(LogLevel::Debug, msg)
#define LOG_ERROR(msg) Logger::Instance().Log(LogLevel::Error, msg)
