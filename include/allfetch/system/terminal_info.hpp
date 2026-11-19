#pragma once

#include "allfetch/system/system_info.hpp"
#include "allfetch/system/terminal/win_terminal_font.hpp"
#include "allfetch/system/terminal/vscode_terminal_font.hpp"
#include "allfetch/system/terminal/gnome_terminal_font.hpp"
#include "allfetch/system/terminal/windows_console_font.hpp"

#include <nlohmann/json.hpp>

#include <cstdlib>
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
        const std::string &gnomeProfileId = {})
    {
        TerminalInfo terminalInfo;

        const auto environment = [](const char *name)
        {
            const char *value = std::getenv(name);
            return value != nullptr ? std::string(value) : std::string{};
        };

        const auto collectFont = [&]()
        {
            if (terminalInfo.name == "Windows Terminal")
            {
                terminal_font_detail::collectWindowsTerminalFont(
                    fontSettings,
                    environment("WT_PROFILE_ID"),
                    terminalInfo);
            }
            else if (terminalInfo.name == "VS Code integrated terminal")
            {
                terminal_font_detail::collectVSCodeFont(
                    fontSettings,
                    terminalInfo);
            }
#if defined(_WIN32)
            else if (terminalInfo.name == "Windows Console")
            {
                terminal_font_detail::collectWindowsConsoleFont(
                    terminalInfo);
            }
#endif
#if defined(__linux__) && defined(ALLFETCH_HAS_GIO)
            else if (terminalInfo.name == "GNOME Terminal")
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

        const auto program = environment("TERM_PROGRAM");

        if (!program.empty() && program != "tmux" &&
            program != "screen")
        {
            terminalInfo.name = program == "vscode"
                                    ? "VS Code integrated terminal"
                                    : program;

            const auto version = environment("TERM_PROGRAM_VERSION");

            if (!version.empty())
            {
                terminalInfo.version = version;
            }

            collectFont();
            return terminalInfo;
        }

        // A multiplexer can retain hints from an outer terminal.
        if (!environment("TMUX").empty() ||
            !environment("STY").empty())
        {
            return terminalInfo;
        }

        if (!environment("WT_SESSION").empty())
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
        }
#endif

        collectFont();
        return terminalInfo;
    }
}
