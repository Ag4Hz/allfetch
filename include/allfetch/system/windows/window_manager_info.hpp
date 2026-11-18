#pragma once

#include "allfetch/system/system_info.hpp"

#include <windows.h>

#include <filesystem>
#include <vector>

namespace allfetch::system
{
    inline WindowManagerInfo getWindowManagerInfo()
    {
        WindowManagerInfo info;

        // This collector targets Windows 10 and Windows 11 desktops.
        info.name = "DWM";

        constexpr const wchar_t *key =
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes";

        DWORD bytes = 0;

        if (RegGetValueW(
                HKEY_CURRENT_USER,
                key,
                L"CurrentTheme",
                RRF_RT_REG_SZ,
                nullptr,
                nullptr,
                &bytes) != ERROR_SUCCESS)
            return info;

        if (bytes == 0 || bytes > 65536 || bytes % sizeof(wchar_t) != 0)
            return info;

        std::vector<wchar_t> buffer(
            bytes / sizeof(wchar_t) + 1, L'\0');

        if (RegGetValueW(
                HKEY_CURRENT_USER,
                key,
                L"CurrentTheme",
                RRF_RT_REG_SZ,
                nullptr,
                buffer.data(),
                &bytes) != ERROR_SUCCESS)
            return info;

        if (buffer.front() == L'\0')
            return info;

        // For example, "C:\...\Custom.theme" becomes "Custom".
        const auto name =
            std::filesystem::path(buffer.data())
                .stem()
                .u8string();

        if (!name.empty())
            info.theme.assign(name.begin(), name.end());

        return info;
    }
}