#pragma once

#include "allfetch/system/system_info.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <sstream>
#include <string>

#if defined(_WIN32)
#include "allfetch/system/windows/program_version.hpp"
#include <Windows.h>
#include <tlhelp32.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace allfetch::system
{
    namespace shell_detail
    {
        inline std::string shellName(std::string executable)
        {
            std::transform(
                executable.begin(), executable.end(),
                executable.begin(),
                [](unsigned char c)
                {
                    return c >= 'A' && c <= 'Z'
                               ? c + ('a' - 'A')
                               : c;
                });

            if (executable.ends_with(".exe"))
            {
                executable.resize(executable.size() - 4);
            }

            if (executable == "bash")
                return "Bash";
            if (executable == "zsh")
                return "Zsh";
            if (executable == "fish")
                return "Fish";
            if (executable == "dash")
                return "Dash";
            if (executable == "ksh" ||
                executable == "ksh93")
                return "KornShell";
            if (executable == "sh")
                return "sh";
            if (executable == "ash")
                return "ash";
            if (executable == "pwsh")
                return "PowerShell";
            if (executable == "powershell")
                return "Windows PowerShell";
            if (executable == "cmd")
                return "Command Prompt";
            if (executable == "nu")
                return "Nushell";
            if (executable == "xonsh")
                return "Xonsh";
            if (executable == "elvish")
                return "Elvish";

            return {};
        }

#if defined(_WIN32)
        struct CloseSnapshot
        {
            void operator()(void *handle) const noexcept
            {
                CloseHandle(handle);
            }
        };

        inline std::string toUtf8(const std::wstring &value)
        {
            if (value.empty())
            {
                return {};
            }

            const int size = WideCharToMultiByte(
                CP_UTF8, 0, value.data(),
                static_cast<int>(value.size()),
                nullptr, 0, nullptr, nullptr);

            if (size <= 0)
            {
                return {};
            }

            std::string text(size, '\0');

            if (WideCharToMultiByte(
                    CP_UTF8, 0, value.data(),
                    static_cast<int>(value.size()),
                    text.data(), size,
                    nullptr, nullptr) == 0)
            {
                return {};
            }

            return text;
        }
#endif

    }

    /* Implementation of the getShellInfo function
    Output:
        ShellInfo
    */
    inline ShellInfo getShellInfo()
    {
        ShellInfo shellInfo;

#if defined(_WIN32)
        HANDLE rawSnapshot = CreateToolhelp32Snapshot(
            TH32CS_SNAPPROCESS, 0);

        if (rawSnapshot == INVALID_HANDLE_VALUE)
        {
            return shellInfo;
        }

        std::unique_ptr<void, shell_detail::CloseSnapshot>
            snapshot(rawSnapshot);

        DWORD pid = GetCurrentProcessId();
        std::set<DWORD> visited;

        for (int depth = 0;
             depth < 64 && pid != 0 && visited.insert(pid).second;
             ++depth)
        {
            PROCESSENTRY32W entry{};
            entry.dwSize = sizeof(entry);
            bool found = false;

            if (Process32FirstW(snapshot.get(), &entry))
            {
                do
                {
                    if (entry.th32ProcessID == pid)
                    {
                        found = true;
                        break;
                    }
                } while (Process32NextW(snapshot.get(), &entry));
            }

            if (!found)
            {
                break;
            }

            if (depth > 0)
            {
                const auto name = shell_detail::shellName(
                    shell_detail::toUtf8(entry.szExeFile));

                if (!name.empty())
                {
                    shellInfo.name = name;

                    HANDLE process = OpenProcess(
                        PROCESS_QUERY_LIMITED_INFORMATION,
                        FALSE, pid);

                    if (process != nullptr)
                    {
                        wchar_t path[32768]{};
                        DWORD length = 32768;

                        const bool success =
                            QueryFullProcessImageNameW(
                                process, 0, path, &length) != 0;

                        CloseHandle(process);

                        if (success)
                        {
                            const std::wstring executable(path, length);
                            shellInfo.executable = shell_detail::toUtf8(executable);
                            if (name != "Windows PowerShell")
                            {
                                const auto version = windows_version_detail::fileVersion(executable);
                                if (!version.empty())
                                    shellInfo.version = version;
                            }
                        }
                    }

                    if (name == "Windows PowerShell")
                    {
                        wchar_t version[64]{};
                        DWORD bytes = sizeof(version);
                        if (RegGetValueW(HKEY_LOCAL_MACHINE,
                                L"SOFTWARE\\Microsoft\\PowerShell\\3\\PowerShellEngine",
                                L"PowerShellVersion", RRF_RT_REG_SZ, nullptr, version, &bytes) == ERROR_SUCCESS)
                        {
                            const auto text = shell_detail::toUtf8(version);
                            if (!text.empty())
                                shellInfo.version = text;
                        }
                    }
                    return shellInfo;
                }
            }

            pid = entry.th32ParentProcessID;
        }

#elif defined(__linux__)
        pid_t pid = getppid();
        std::set<pid_t> visited;

        for (int depth = 0;
             depth < 64 && pid > 0 && visited.insert(pid).second;
             ++depth)
        {
            const auto directory = std::filesystem::path("/proc") /
                                   std::to_string(pid);

            std::error_code error;
            const auto executable = std::filesystem::read_symlink(
                directory / "exe", error);

            std::string executableName;

            if (!error)
            {
                executableName = executable.filename().string();
            }
            else
            {
                std::ifstream command(directory / "comm");
                std::getline(command, executableName);
            }

            const auto name = shell_detail::shellName(executableName);

            if (!name.empty())
            {
                shellInfo.name = name;

                if (!error)
                {
                    shellInfo.executable = executable.string();
                }

                return shellInfo;
            }

            std::ifstream status(directory / "status");
            std::string line;
            pid_t parent = 0;

            while (std::getline(status, line))
            {
                if (line.starts_with("PPid:"))
                {
                    std::istringstream fields(line.substr(5));

                    if (!(fields >> parent))
                    {
                        parent = 0;
                    }

                    break;
                }
            }

            pid = parent;
        }
#endif

        return shellInfo;
    }
}
