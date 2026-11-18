#pragma once

#include "allfetch/system/system_info.hpp"

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
    inline TerminalInfo getTerminalInfo()
    {
        TerminalInfo terminalInfo;

        const auto environment = [](const char *name)
        {
            const char *value = std::getenv(name);
            return value != nullptr ? std::string(value) : std::string{};
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

        return terminalInfo;
    }
}