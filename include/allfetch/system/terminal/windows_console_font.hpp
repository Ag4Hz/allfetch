#pragma once

#include "allfetch/system/system_info.hpp"

#if defined(_WIN32)

#include <windows.h>

#include <algorithm>
#include <string>

namespace allfetch::system::terminal_font_detail
{
    inline void collectWindowsConsoleFont(TerminalInfo &info)
    {
        CONSOLE_FONT_INFOEX font{};
        font.cbSize = sizeof(font);

        if (!GetCurrentConsoleFontEx(
                GetStdHandle(STD_OUTPUT_HANDLE),
                FALSE,
                &font))
            return;

        const wchar_t *end = std::find(
            font.FaceName,
            font.FaceName + LF_FACESIZE,
            L'\0');

        const int length =
            static_cast<int>(end - font.FaceName);

        if (length == 0)
            return;

        const int bytes = WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS,
            font.FaceName, length,
            nullptr, 0, nullptr, nullptr);

        if (bytes <= 0)
            return;

        std::string name(bytes, '\0');

        if (!WideCharToMultiByte(
                CP_UTF8, WC_ERR_INVALID_CHARS,
                font.FaceName, length,
                name.data(), bytes,
                nullptr, nullptr))
            return;

        info.font = name;
        info.fontSource = "Windows Console API";
    }
}

#endif