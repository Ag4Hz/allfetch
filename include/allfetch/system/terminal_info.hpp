#pragma once

#include "allfetch/system/system_info.hpp"
#include "allfetch/system/terminal/win_terminal_font.hpp"
#include "allfetch/system/terminal/vscode_terminal_font.hpp"
#include "allfetch/system/terminal/gnome_terminal_font.hpp"
#include "allfetch/system/terminal/windows_console_font.hpp"

#include <nlohmann/json.hpp>

#include <string>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace allfetch::system
{
    /* Implementation of the getTerminalInfo function
    Output:
        TerminalInfo
    */
    inline TerminalInfo getTerminalInfo(
        const nlohmann::json &fontSettings = nlohmann::json::object(),
        [[maybe_unused]] const std::string &gnomeProfileId = {},
        bool collectFont = true,
#if defined(_WIN32)
        const std::filesystem::path &host = terminal_font_detail::windowsTerminalExecutable(windows_platform::readProcesses()))
#else
        const std::filesystem::path &host = {})
#endif
    {
        TerminalInfo terminalInfo;

        using terminal_font_detail::environment;

        const auto collectDetails = [&]()
        {
            if (terminalInfo.name == "Windows Terminal")
            {
#if defined(_WIN32)
                if (terminalInfo.version == "Unknown")
                {
                    const auto version = terminal_font_detail::windowsTerminalVersion(host);
                    if (!version.empty())
                        terminalInfo.version = version;
                }
#endif
                if (!collectFont)
                    return;
                terminal_font_detail::collectWindowsTerminalFont(
                    fontSettings,
                    environment("WT_PROFILE_ID"),
                    terminalInfo, host);
            }
            else if (collectFont && terminalInfo.name == "VS Code integrated terminal")
            {
                terminal_font_detail::collectVSCodeFont(
                    fontSettings,
                    terminalInfo);
            }
#if defined(_WIN32)
            else if (terminalInfo.name == "Windows Console")
            {
                wchar_t systemDirectory[MAX_PATH]{};
                const auto length = GetSystemDirectoryW(systemDirectory, MAX_PATH);
                if (length > 0 && length < MAX_PATH)
                {
                    const auto version = windows_version_detail::fileVersion(
                        std::filesystem::path(systemDirectory) / L"conhost.exe");
                    if (!version.empty())
                        terminalInfo.version = version;
                }
                if (!collectFont)
                    return;
                terminal_font_detail::collectWindowsConsoleFont(
                    terminalInfo);
            }
#endif
#if defined(__linux__) && defined(ALLFETCH_HAS_GIO)
            else if (collectFont && terminalInfo.name == "GNOME Terminal")
            {
                terminal_font_detail::collectGnomeTerminalFont(
                    gnomeProfileId,
                    terminalInfo);
            }
#endif
        };

        // These hints can describe the SSH client environment rather
        // than a terminal application running on this machine.
        if (!environment("SSH_CONNECTION").empty() ||
            !environment("SSH_TTY").empty())
        {
            return terminalInfo;
        }

        // Multiplexers can retain environment hints from an outer terminal.
        if (!environment("TMUX").empty() || !environment("STY").empty())
            return terminalInfo;

        const auto program = environment("TERM_PROGRAM");

        if (!program.empty() && program != "tmux" &&
            program != "screen")
        {
            const auto normalized = terminal_font_detail::normalizeGuid(program);
            if (normalized == "vscode")
                terminalInfo.name = "VS Code integrated terminal";
            else if (normalized == "windowsterminal" || normalized == "windows terminal")
                terminalInfo.name = "Windows Terminal";
            else
                terminalInfo.name = program;

            const auto version = environment("TERM_PROGRAM_VERSION");

            if (!version.empty())
            {
                terminalInfo.version = version;
            }

            collectDetails();
            return terminalInfo;
        }

        if (!environment("WT_SESSION").empty() ||
            !environment("WT_PROFILE_ID").empty())
        {
            terminalInfo.name = "Windows Terminal";
        }
#if defined(__linux__)
        else if (!environment("GNOME_TERMINAL_SERVICE").empty() &&
                 !environment("GNOME_TERMINAL_SCREEN").empty())
        {
            terminalInfo.name = "GNOME Terminal";
        }
#endif
        else if (environment("TERM") == "linux")
        {
            terminalInfo.name = "Linux virtual console";
        }

#if defined(_WIN32)
        if (terminalInfo.name == "Unknown")
        {
            const HWND window = GetConsoleWindow();

            if (window != nullptr && IsWindowVisible(window))
            {
                terminalInfo.name = "Windows Console";
            }
            else if (!host.empty())
            {
                terminalInfo.name = "Windows Terminal";
            }
            else if (window != nullptr &&
                     !terminal_font_detail::defaultWindowsTerminalPackage().empty())
            {
                // Delegated console launches need not inherit WT_SESSION.
                // This is the configured default host, not a live process query.
                terminalInfo.name = "Windows Terminal";
            }
        }
#endif

        collectDetails();
        return terminalInfo;
    }
}
