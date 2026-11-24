#pragma once

#include "allfetch/system/system_info.hpp"

#include "allfetch/system/windows/platform.hpp"
#include <shlwapi.h>
#include <uxtheme.h>

#include <algorithm>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
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

        using windows_platform::utf8;

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

        inline std::string visualStyle()
        {
            wchar_t path[32768]{};
            if (FAILED(GetCurrentThemeName(path, 32768, nullptr, 0, nullptr, 0)))
                return {};

            auto name = std::filesystem::path(path).stem().native();
            if (_wcsicmp(name.c_str(), L"aero") == 0)
            {
                using VersionFunction = LONG(WINAPI *)(OSVERSIONINFOW *);
                const auto module = GetModuleHandleW(L"ntdll.dll");
                const auto versionFunction = module != nullptr
                    ? reinterpret_cast<VersionFunction>(GetProcAddress(module, "RtlGetVersion"))
                    : nullptr;
                OSVERSIONINFOW version{};
                version.dwOSVersionInfoSize = sizeof(version);
                if (versionFunction != nullptr && versionFunction(&version) == 0)
                {
                    if (version.dwMajorVersion >= 10 && version.dwBuildNumber >= 22000)
                        return "Fluent";
                    if (version.dwMajorVersion > 6 ||
                        (version.dwMajorVersion == 6 && version.dwMinorVersion >= 2))
                        return "Metro";
                }
                return "Aero";
            }
            return utf8(name);
        }

        inline void collectDesktopIcons(DesktopAppearanceInfo &info)
        {
            // The global switch overrides the selection of individual icons.
            if (const auto hidden = registryNumber(
                    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
                    L"HideIcons"); hidden && *hidden == 1)
            {
                info.desktopIconsKnown = true;
                return;
            }

            constexpr const wchar_t *modern =
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\HideDesktopIcons\\NewStartPanel";
            constexpr const wchar_t *classic =
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\HideDesktopIcons\\ClassicStartMenu";
            HKEY key = nullptr;
            auto status = RegOpenKeyExW(HKEY_CURRENT_USER, modern, 0, KEY_QUERY_VALUE, &key);
            if (status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND)
                status = RegOpenKeyExW(HKEY_CURRENT_USER, classic, 0, KEY_QUERY_VALUE, &key);
            if (status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND)
            {
                info.desktopIcons = {"Recycle Bin"};
                info.desktopIconsKnown = true;
                return;
            }
            if (status != ERROR_SUCCESS)
                return;

            const struct IconSetting
            {
                const wchar_t *id;
                const char *name;
                DWORD hiddenByDefault;
            } icons[] = {
                {L"{20D04FE0-3AEA-1069-A2D8-08002B30309D}", "This PC", 1},
                {L"{59031A47-3F72-44A7-89C5-5595FE6B30EE}", "User's Files", 1},
                {L"{F02C1A0D-BE21-4350-88B0-7367FC96EF3C}", "Remote Network", 1},
                {L"{645FF040-5081-101B-9F08-00AA002F954E}", "Recycle Bin", 0},
                {L"{5399E694-6CE5-4D6C-8FCE-1D8870FDCBA0}", "Control Panel", 1}};

            std::vector<std::string> visible;
            bool valid = true;
            for (const auto &icon : icons)
            {
                DWORD hidden = icon.hiddenByDefault;
                DWORD bytes = sizeof(hidden);
                status = RegGetValueW(key, nullptr, icon.id, RRF_RT_REG_DWORD,
                                      nullptr, &hidden, &bytes);
                if (status == ERROR_FILE_NOT_FOUND)
                    hidden = icon.hiddenByDefault;
                else if (status != ERROR_SUCCESS || bytes != sizeof(hidden))
                {
                    valid = false;
                    break;
                }
                if (hidden == 0)
                    visible.emplace_back(icon.name);
            }
            RegCloseKey(key);
            if (valid)
            {
                info.desktopIcons = std::move(visible);
                info.desktopIconsKnown = true;
            }
        }
    }

    inline DesktopAppearanceInfo getDesktopAppearanceInfo()
    {
        DesktopAppearanceInfo info;
        constexpr const wchar_t *personalize =
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
        constexpr const wchar_t *cursors = L"Control Panel\\Cursors";

        const auto style = appearance_detail::visualStyle();
        if (!style.empty())
            info.theme = style;
        appearance_detail::collectDesktopIcons(info);

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
