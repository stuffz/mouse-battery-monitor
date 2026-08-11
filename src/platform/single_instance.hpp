#pragma once

// Both backends rely on the OS releasing the underlying object when the process
// dies for any reason, so neither needs stale-lock detection.

#ifdef _WIN32

#include "core/logger.hpp"
#include "platform/platform.hpp"

#include <string>

// The name is in the "Local\" namespace, scoped to the logon session, so each
// user and Remote Desktop session gets an independent instance. "Global\" would
// let one user's tray icon block another's.
class SingleInstanceLock
{
public:
    enum class Result
    {
        Acquired,
        AlreadyRunning,
        Unavailable
    };

    SingleInstanceLock() = default;

    ~SingleInstanceLock() { release(); }

    SingleInstanceLock(const SingleInstanceLock &) = delete;
    SingleInstanceLock &operator=(const SingleInstanceLock &) = delete;

    Result acquire()
    {
        // bInitialOwner FALSE: a presence flag only, never waited on, so there
        // is no ownership to abandon if this process dies.
        handle = CreateMutexW(nullptr, FALSE, MUTEX_NAME);

        // Must be read before any other call can overwrite it.
        const DWORD error = GetLastError();

        if (handle == nullptr)
        {
            LOG_ERROR("CreateMutexW failed: " + std::to_string(error));
            // Fail open: no guard is a better outcome than refusing to start.
            return Result::Unavailable;
        }

        if (error == ERROR_ALREADY_EXISTS)
        {
            release();
            return Result::AlreadyRunning;
        }

        return Result::Acquired;
    }

private:
    static constexpr wchar_t MUTEX_NAME[] = L"Local\\MouseBatteryMonitor.SingleInstance";

    HANDLE handle = nullptr;

    void release()
    {
        if (handle != nullptr)
        {
            CloseHandle(handle);
            handle = nullptr;
        }
    }
};

#else

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>

#include "core/logger.hpp"

// flock() is tied to the open file description, so the kernel releases it when
// the fd closes - including on a crash or SIGKILL. That is why this is preferred
// over a pid file or QLockFile.
class SingleInstanceLock
{
public:
    enum class Result
    {
        Acquired,
        AlreadyRunning,
        Unavailable
    };

    SingleInstanceLock() = default;

    ~SingleInstanceLock() { release(); }

    SingleInstanceLock(const SingleInstanceLock &) = delete;
    SingleInstanceLock &operator=(const SingleInstanceLock &) = delete;

    Result acquire(const std::filesystem::path &path)
    {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);

        fd = ::open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
        if (fd < 0)
        {
            LOG_ERROR("Could not open lock file " + path.string() + ": " + std::strerror(errno));
            // Fail open: a missing lock is a worse outcome than no guard.
            return Result::Unavailable;
        }

        if (::flock(fd, LOCK_EX | LOCK_NB) == 0)
        {
            writePid();
            return Result::Acquired;
        }

        const int err = errno;

        if (err == EWOULDBLOCK)
        {
            holder = readHolder();
            release();
            return Result::AlreadyRunning;
        }

        LOG_ERROR(std::string("flock on the instance lock failed: ") + std::strerror(err));
        release();
        return Result::Unavailable;
    }

    const std::string &holderPid() const { return holder; }

private:
    int fd = -1;
    std::string holder;

    void release()
    {
        if (fd >= 0)
        {
            // Closing the fd drops the flock; no explicit LOCK_UN needed.
            ::close(fd);
            fd = -1;
        }
    }

    void writePid()
    {
        const std::string pid = std::to_string(::getpid()) + "\n";

        if (::ftruncate(fd, 0) != 0 ||
            ::pwrite(fd, pid.data(), pid.size(), 0) != static_cast<ssize_t>(pid.size()))
        {
            // Only cosmetic: the lock itself is what enforces exclusivity.
            LOG_DEBUG("Could not record pid in the instance lock");
        }
    }

    std::string readHolder() const
    {
        char buffer[32] = {0};
        const ssize_t n = ::pread(fd, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0)
        {
            return {};
        }

        std::string pid(buffer, static_cast<size_t>(n));
        while (!pid.empty() && (pid.back() == '\n' || pid.back() == '\r'))
        {
            pid.pop_back();
        }
        return pid;
    }
};

#endif
