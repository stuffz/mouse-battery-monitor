#pragma once

#include "core/device_info.hpp" // IWYU pragma: export

#ifdef _WIN32
#include "core/hid_device_win32.hpp" // IWYU pragma: export
#else
#include "core/hid_device_linux.hpp" // IWYU pragma: export
#endif
