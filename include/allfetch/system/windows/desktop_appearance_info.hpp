#pragma once

#include "allfetch/system/system_info.hpp"

#include <windows.h>
#include <shlwapi.h>

#include <algorithm>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace allfetch::system
{
    namespace appearance_detail
    {
        inline std::optional<std::wstring> registryString(
            const wchar_t *key, const wchar_t *name)
        {
            DWORD bytes = 0;
            if (RegGetValueW(HKEY_CURRENT_USER, key, name, RRF_RT_REG_SZ,
                             nullptr, nullptr, &bytes) != ERROR_SUCCESS ||
                bytes > 65536 || bytes % sizeof(wchar_t) != 0)
                return std::nullopt;

            std::vector<wchar_t> buffer(bytes / sizeof(wchar_t) + 1, L'\0');
            if (RegGetValueW(HKEY_CURRENT_USER, key, name, RRF_RT_REG_SZ,
                             nullptr, buffer.data(), &bytes) != ERROR_SUCCESS)
                return std::nullopt;

            return std::wstring(buffer.data());
        }

        inline std::optional<DWORD> registryNumber(
            const wchar_t *key, const wchar_t *name)
        {
            DWORD value = 0;
            DWORD bytes = sizeof(value);
            if (RegGetValueW(HKEY_CURRENT_USER, key, name, RRF_RT_REG_DWORD,
                             nullptr, &value, &bytes) != ERROR_SUCCESS ||
                bytes != sizeof(value))
                return std::nullopt;
            return value;
        }

        inline std::string utf8(std::wstring_view text)
        {
            if (text.empty())
                return {};

            const int length = static_cast<int>(text.size());
            const int bytes = WideCharToMultiByte(
                CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), length,
                nullptr, 0, nullptr, nullptr);
            if (bytes <= 0)
                return {};

            std::string result(bytes, '\0');
            if (!WideCharToMultiByte(
                    CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), length,
                    result.data(), bytes, nullptr, nullptr))
                return {};
            return result;
        }

        inline std::string displayName(const std::wstring &value)
        {
            if (value.empty())
                return {};
            if (value.front() != L'@')
                return utf8(value);

            wchar_t resolved[4096]{};
            if (SUCCEEDED(SHLoadIndirectString(value.c_str(), resolved, 4096, nullptr)))
                return utf8(resolved);
            return {};
        }
    }

    inline DesktopAppearanceInfo getDesktopAppearanceInfo()
    {
        DesktopAppearanceInfo info;
        constexpr const wchar_t *themes =
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes";
        constexpr const wchar_t *personalize =
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
        constexpr const wchar_t *cursors = L"Control Panel\\Cursors";

        if (const auto theme = appearance_detail::registryString(themes, L"CurrentTheme");
            theme && !theme->empty())
        {
            wchar_t name[4096]{};
            const DWORD length = GetPrivateProfileStringW(
                L"Theme", L"DisplayName", L"", name, 4096, theme->c_str());
            std::string resolved;
            if (length > 0 && length < 4095)
                resolved = appearance_detail::displayName(name);
            if (resolved.empty())
                resolved = appearance_detail::utf8(
                    std::filesystem::path(*theme).stem().native());
            if (!resolved.empty())
                info.theme = resolved;
        }

        // System UI preference; application light/dark mode is a separate setting.
        if (const auto light = appearance_detail::registryNumber(personalize, L"SystemUsesLightTheme");
            light && *light <= 1)
            info.colorScheme = *light == 1 ? "Light" : "Dark";

        HIGHCONTRASTW contrast{};
        contrast.cbSize = sizeof(contrast);
        if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0) &&
            (contrast.dwFlags & HCF_HIGHCONTRASTON))
            info.colorScheme = "High contrast";

        if (const auto scheme = appearance_detail::registryString(cursors, nullptr))
        {
            const auto name = appearance_detail::displayName(*scheme);
            if (!name.empty())
                info.cursorTheme = name;
            else if (scheme->empty())
            {
                const auto source = appearance_detail::registryNumber(cursors, L"Scheme Source");
                if (source && *source == 0)
                    info.cursorTheme = "None";
                else if (source && *source == 2)
                    info.cursorTheme = "Windows Default";
            }
        }

        if (const auto size = appearance_detail::registryNumber(cursors, L"CursorBaseSize");
            size && *size > 0 && *size <= static_cast<DWORD>((std::numeric_limits<int>::max)()))
            info.cursorSize = static_cast<int>(*size);

        NONCLIENTMETRICSW metrics{};
        metrics.cbSize = sizeof(metrics);
        if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0))
        {
            const auto *name = metrics.lfMessageFont.lfFaceName;
            const auto *end = std::find(name, name + LF_FACESIZE, L'\0');
            const auto font = appearance_detail::utf8(
                std::wstring_view(name, static_cast<std::size_t>(end - name)));
            if (!font.empty())
                info.font = font;
        }

        // Windows has per-object icon assignments, not one global icon theme.
        return info;
    }
}
