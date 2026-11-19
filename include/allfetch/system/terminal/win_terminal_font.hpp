#pragma once

#include "allfetch/system/system_info.hpp"
#include "allfetch/system/terminal/font_settings.hpp"

#if defined(_WIN32)
#include <windows.h>
#include <tlhelp32.h>
#endif

#include <cctype>
#include <string>
#include <string_view>
#include <unordered_map>
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
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, KEY_READ, &key)
            != ERROR_SUCCESS)
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

    inline const std::filesystem::path &windowsTerminalExecutable()
    {
        static const std::filesystem::path executable = []() -> std::filesystem::path
        {
#if defined(_WIN32)
            const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (snapshot == INVALID_HANDLE_VALUE)
                return {};

            std::unordered_map<DWORD, std::pair<DWORD, std::wstring>> processes;
            PROCESSENTRY32W entry{};
            entry.dwSize = sizeof(entry);
            for (BOOL found = Process32FirstW(snapshot, &entry); found;
                 found = Process32NextW(snapshot, &entry))
                processes.emplace(entry.th32ProcessID,
                                  std::make_pair(entry.th32ParentProcessID, entry.szExeFile));
            CloseHandle(snapshot);

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

                    if (_wcsicmp(process->second.second.c_str(), L"WindowsTerminal.exe") == 0)
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
                    pid = process->second.first;
                }
            }
#endif
            return {};
        }();
        return executable;
    }

    inline nlohmann::json windowsTerminalSettings()
    {
#if defined(_WIN32)
        const auto &host = windowsTerminalExecutable();
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
        TerminalInfo &info)
    {
        const auto configuration = settings.empty() ? windowsTerminalSettings() : settings;
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
