#pragma once

// Lets the portable layers be written once against the Windows spellings.

#ifdef _WIN32

#include <string>
#include <windows.h>

using NativePath = std::wstring;

#else

#include <cstdint>
#include <string>

using BYTE = std::uint8_t;
using USHORT = std::uint16_t;
using DWORD = std::uint32_t;

using NativePath = std::string;

#endif
