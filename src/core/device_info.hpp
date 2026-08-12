#pragma once

#include "platform/platform.hpp"

// One entry per top-level HID application collection. Windows exposes each as
// its own device interface; the Linux backend reproduces that from the
// descriptor.
struct DeviceInfo
{
    NativePath path;
    USHORT usagePage = 0;
    USHORT usage = 0;
};
