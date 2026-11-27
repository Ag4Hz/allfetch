#pragma once

#include "allfetch/system/system_info.hpp"
#include "allfetch/system/terminal/font_settings.hpp"

#if defined(_WIN32)
#include "allfetch/system/windows/program_version.hpp"
#include <windows.h>
#include <appmodel.h>
#include "allfetch/system/windows/process_snapshot.hpp"
#endif

#include <cctype>
#include <string>
#include <string_view>
#include <utility>

namespace allfetch::system::terminal_font_detail
{
    inline std::string normalizeGuid(std::string value)
    {
        std::string result;

        for (unsigned char character : value)
        {
            if (character != '{' && character != '}')
            {
                result.push_back(
                    static_cast<char>(std::tolower(character)));
            }
        }

        return result;
    }

    inline std::wstring defaultWindowsTerminalPackage()
    {
#if defined(_WIN32)
        wchar_t delegation[64]{};
        DWORD bytes = sizeof(delegation);
        if (RegGetValueW(
                HKEY_CURRENT_USER, L"Console\\%%Startup", L"DelegationTerminal",
                RRF_RT_REG_SZ, nullptr, delegation, &bytes) != ERROR_SUCCESS)
            return {};

        const std::wstring path =
            std::wstring(L"SOFTWARE\\Classes\\PackagedCom\\ClassIndex\\") + delegation;
        HKEY key = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS)
            return {};

        wchar_t package[512]{};
        DWORD length = 512;
        const auto status = RegEnumKeyExW(
            key, 0, package, &length, nullptr, nullptr, nullptr, nullptr);
        RegCloseKey(key);

        if (status == ERROR_SUCCESS &&
            std::wstring_view(package, length).starts_with(L"Microsoft.WindowsTerminal"))
            return std::wstring(package, length);
#endif
        return {};
    }

#if defined(_WIN32)
    inline std::filesystem::path windowsTerminalExecutable(const windows_platform::ProcessList &processes)
    {
        DWORD consolePid = 0;
        GetWindowThreadProcessId(GetConsoleWindow(), &consolePid);
        for (DWORD start : {GetCurrentProcessId(), consolePid})
        {
            DWORD pid = start;
            for (unsigned depth = 0; pid != 0 && depth < 64; ++depth)
            {
                const auto process = processes.find(pid);
                if (process == processes.end())
                    break;

                if (_wcsicmp(process->second.executable.c_str(), L"WindowsTerminal.exe") == 0)
                {
                    const HANDLE handle = OpenProcess(
                        PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
                    if (handle != nullptr)
                    {
                        wchar_t path[32768]{};
                        DWORD length = 32768;
                        const BOOL found = QueryFullProcessImageNameW(handle, 0, path, &length);
                        CloseHandle(handle);
                        if (found)
                            return std::wstring(path, length);
                    }
                    return L"WindowsTerminal.exe";
                }
                pid = process->second.parent;
            }
        }
        return {};
    }

    inline PACKAGE_VERSION packageVersion(const wchar_t *fullName)
    {
        UINT32 bytes = 0;
        if (PackageIdFromFullName(fullName, PACKAGE_INFORMATION_BASIC, &bytes, nullptr) !=
            ERROR_INSUFFICIENT_BUFFER)
            return {};
        std::vector<BYTE> buffer(bytes);
        if (bytes < sizeof(PACKAGE_ID) ||
            PackageIdFromFullName(fullName, PACKAGE_INFORMATION_BASIC, &bytes, buffer.data()) != ERROR_SUCCESS)
            return {};
        return reinterpret_cast<const PACKAGE_ID *>(buffer.data())->version;
    }

    inline PACKAGE_VERSION installedTerminalVersion(const wchar_t *family)
    {
        UINT32 count = 0;
        UINT32 length = 0;
        if (GetPackagesByPackageFamily(family, &count, nullptr, &length, nullptr) != ERROR_INSUFFICIENT_BUFFER)
            return {};
        std::vector<wchar_t *> names(count);
        std::vector<wchar_t> storage(length);
        if (GetPackagesByPackageFamily(family, &count, names.data(), &length, storage.data()) != ERROR_SUCCESS)
            return {};

        PACKAGE_VERSION latest{};
        for (UINT32 i = 0; i < count; ++i)
        {
            const auto version = packageVersion(names[i]);
            if (version.Version > latest.Version)
                latest = version;
        }
        return latest;
    }

    inline std::string windowsTerminalVersion(const std::filesystem::path &executable)
    {
        const auto format = [](PACKAGE_VERSION version)
        {
            return windows_version_detail::formatVersion(
                version.Major, version.Minor, version.Build, version.Revision);
        };

        if (executable.is_absolute())
        {
            const auto folder = executable.parent_path().filename().wstring();
            if (folder.starts_with(L"Microsoft.WindowsTerminal"))
                if (const auto version = packageVersion(folder.c_str()); version.Version != 0)
                    return format(version);
            // The live executable identifies portable and unpackaged installations.
            return windows_version_detail::fileVersion(executable);
        }

        const auto configured = defaultWindowsTerminalPackage();
        if (!configured.empty())
            if (const auto version = packageVersion(configured.c_str()); version.Version != 0)
                return format(version);

        constexpr auto stableFamily = L"Microsoft.WindowsTerminal_8wekyb3d8bbwe";
        constexpr auto previewFamily = L"Microsoft.WindowsTerminalPreview_8wekyb3d8bbwe";
        const auto stable = installedTerminalVersion(stableFamily);
        const auto preview = installedTerminalVersion(previewFamily);
        if (stable.Version && !preview.Version)
            return format(stable);
        if (preview.Version && !stable.Version)
            return format(preview);
        return {};
    }
#endif

    inline nlohmann::json windowsTerminalSettings([[maybe_unused]] const std::filesystem::path &host)
    {
#if defined(_WIN32)
        std::error_code portableError;
        if (host.has_parent_path() &&
            std::filesystem::exists(host.parent_path() / ".portable", portableError))
            return readFontSettings(host.parent_path() / "settings" / "settings.json");

        const auto root = environmentPath("LOCALAPPDATA");
        if (root.empty())
            return nullptr;

        const auto stable = root / "Packages" /
                            "Microsoft.WindowsTerminal_8wekyb3d8bbwe" /
                            "LocalState" / "settings.json";
        const auto preview = root / "Packages" /
                             "Microsoft.WindowsTerminalPreview_8wekyb3d8bbwe" /
                             "LocalState" / "settings.json";
        const auto unpackaged = root / "Microsoft" / "Windows Terminal" / "settings.json";
        const bool preferPreview = host.native().find(L"Microsoft.WindowsTerminalPreview_") !=
                                       std::wstring::npos ||
                                   (host.empty() && defaultWindowsTerminalPackage()
                                                        .starts_with(L"Microsoft.WindowsTerminalPreview"));

        for (const auto &path : {preferPreview ? preview : stable,
                                 unpackaged,
                                 preferPreview ? stable : preview})
        {
            std::error_code error;
            if (std::filesystem::exists(path, error))
                return readFontSettings(path);
        }
#endif
        return nullptr;
    }

    inline std::string windowsFontFace(
        const nlohmann::json &object)
    {
        if (!object.is_object())
            return {};

        const auto font = object.find("font");

        if (font != object.end())
        {
            const auto face = stringSetting(*font, "face");

            if (!face.empty())
                return face;
        }

        // Older Windows Terminal settings.
        return stringSetting(object, "fontFace");
    }

    inline void collectWindowsTerminalFont(
        const nlohmann::json &settings,
        const std::string &profileId,
        TerminalInfo &info, const std::filesystem::path &host)
    {
        const auto configuration = settings.empty() ? windowsTerminalSettings(host) : settings;
        if (!configuration.is_object())
            return;

        std::string selectedId = profileId;
        if (selectedId.empty())
            selectedId = stringSetting(configuration, "defaultProfile");

        std::string font;
        const auto profiles = configuration.find("profiles");
        if (profiles != configuration.end() && profiles->is_object())
        {
            const auto defaults = profiles->find("defaults");
            if (defaults != profiles->end())
                font = windowsFontFace(*defaults);

            const auto list = profiles->find("list");
            if (!selectedId.empty() && list != profiles->end() && list->is_array())
            {
                for (const auto &profile : *list)
                {
                    if (normalizeGuid(stringSetting(profile, "guid")) !=
                        normalizeGuid(selectedId))
                        continue;

                    const auto overrideFont = windowsFontFace(profile);
                    if (!overrideFont.empty())
                        font = overrideFont;
                    break;
                }
            }
        }

        if (font.empty())
        {
            info.font = "Cascadia Mono";
            info.fontSource = "Windows Terminal built-in default (configured)";
        }
        else
        {
            info.font = font;
            info.fontSource = profileId.empty()
                                  ? "Windows Terminal default-profile settings"
                                  : "Windows Terminal profile settings";
        }
    }
}
