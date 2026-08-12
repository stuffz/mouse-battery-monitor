#pragma once

#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <vector>

namespace paths
{
namespace fs = std::filesystem;

inline fs::path ExecutableDir()
{
    std::error_code ec;
    const fs::path exe = fs::read_symlink("/proc/self/exe", ec);
    return ec ? fs::current_path() : exe.parent_path();
}

inline fs::path EnvDir(const char *name, const fs::path &fallback)
{
    const char *value = std::getenv(name);
    return (value && *value) ? fs::path(value) : fallback;
}

inline fs::path HomeDir()
{
    return EnvDir("HOME", "/tmp");
}

inline fs::path ConfigDir()
{
    return EnvDir("XDG_CONFIG_HOME", HomeDir() / ".config") / "mouse-battery-monitor";
}

inline fs::path StateDir()
{
    return EnvDir("XDG_STATE_HOME", HomeDir() / ".local" / "state") / "mouse-battery-monitor";
}

inline fs::path FirstExisting(const std::vector<fs::path> &candidates, const fs::path &fallback)
{
    std::error_code ec;
    for (const auto &candidate : candidates)
    {
        if (fs::exists(candidate, ec))
        {
            return candidate;
        }
    }
    return fallback;
}

inline fs::path ResourceDir()
{
    const char *env = std::getenv("MBM_RESOURCE_DIR");
    if (env && *env)
    {
        return fs::path(env);
    }

    const fs::path exeDir = ExecutableDir();
    const fs::path installed =
        exeDir.parent_path() / "share" / "mouse-battery-monitor" / "resources";

    return FirstExisting({exeDir / "resources", installed,
                          "/usr/share/mouse-battery-monitor/resources",
                          "/usr/local/share/mouse-battery-monitor/resources"},
                         exeDir / "resources");
}

inline fs::path ConfigPath()
{
    const char *env = std::getenv("MBM_CONFIG");
    if (env && *env)
    {
        return fs::path(env);
    }

    const fs::path exeDir = ExecutableDir();

    return FirstExisting({ConfigDir() / "config.ini", exeDir / "config.ini", "config.ini"},
                         ConfigDir() / "config.ini");
}

inline fs::path RuntimeDir()
{
    const char *dir = std::getenv("XDG_RUNTIME_DIR");
    return (dir && *dir) ? fs::path(dir) : StateDir();
}

inline fs::path LockPath()
{
    return RuntimeDir() / "mouse-battery-monitor.lock";
}

inline fs::path LogPath()
{
    std::error_code ec;
    const fs::path dir = StateDir();
    fs::create_directories(dir, ec);
    return ec ? fs::path("battery_monitor.log") : dir / "battery_monitor.log";
}
} // namespace paths
