#include <exception>
#include <iostream>
#include <string>

#include "core/application.hpp"
#include "core/logger.hpp"

#ifndef BUILD_DATE
#define BUILD_DATE "unknown"
#endif

#ifndef GIT_HASH
#define GIT_HASH "unknown"
#endif

int main(int argc, char **argv)
{
    try
    {
        auto &app = Application::instance();
        app.setBuildInfo(BUILD_DATE, GIT_HASH);
        return app.run(argc, argv);
    }
    catch (const std::exception &ex)
    {
        LOG_ERROR(std::string("Unhandled exception: ") + ex.what());
        std::cerr << "Unhandled exception: " << ex.what() << "\n";
        return 1;
    }
    catch (...)
    {
        LOG_ERROR("Unhandled unknown exception");
        std::cerr << "Unhandled unknown exception\n";
        return 1;
    }
}
