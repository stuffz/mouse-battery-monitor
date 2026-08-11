#pragma once

#ifdef _WIN32
#include "core/application_win32.hpp" // IWYU pragma: export
#else
#include "core/application_linux.hpp" // IWYU pragma: export
#endif
