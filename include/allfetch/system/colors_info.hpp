#pragma once

#include "allfetch/system/system_info.hpp"

#include <cstdio>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace allfetch::system
{
    inline ColorsInfo getColorsInfo()
    {
        ColorsInfo info;

#if defined(_WIN32)
        // A pseudoconsole's table may differ from the terminal theme.
        const HWND window = GetConsoleWindow();

        if (window == nullptr || !IsWindowVisible(window))
            return info;

        CONSOLE_SCREEN_BUFFER_INFOEX console{};
        console.cbSize = sizeof(console);

        if (!GetConsoleScreenBufferInfoEx(
                GetStdHandle(STD_OUTPUT_HANDLE), &console))
            return info;

        for (unsigned index = 0; index < 16; ++index)
        {
            // ANSI and Windows use different red/blue bit positions.
            const unsigned windowsIndex =
                ((index & 1u) << 2u) | (index & 2u) | ((index & 4u) >> 2u) | (index & 8u);

            const COLORREF color =
                console.ColorTable[windowsIndex];

            char hex[8]{};

            std::snprintf(
                hex, sizeof(hex), "#%02X%02X%02X",
                static_cast<unsigned>(GetRValue(color)),
                static_cast<unsigned>(GetGValue(color)),
                static_cast<unsigned>(GetBValue(color)));

            info.hexValues[index] = hex;
        }
#endif

        return info;
    }
}