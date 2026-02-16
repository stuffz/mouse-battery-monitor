#pragma once

#include <windows.h>
#include <iostream>
#include <limits>
#include "logger.hpp"

class DebugConsole
{
public:
    DebugConsole() = default;

    void attach()
    {
        if (attached)
            return;

        AllocConsole();
        FILE *fp = nullptr;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        freopen_s(&fp, "CONIN$", "r", stdin);
        attached = true;

        LOG_INFO("Debug console attached");
    }

    void waitForExit()
    {
        if (!attached)
            return;

        std::cout << "\nPress Enter to exit..." << std::flush;
        std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
    }

    bool isAttached() const { return attached; }

private:
    bool attached = false;
};
