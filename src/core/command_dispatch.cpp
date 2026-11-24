#include "allfetch/core/command_dispatch.hpp"
#include "allfetch/core/argparser.hpp"
#include "allfetch/render/render.hpp"
#include "allfetch/render/logo_selector.hpp"
#include "allfetch/system/system_info.hpp"
#include <exception>
#include <iostream>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace allfetch::core
{
#if defined(_WIN32)
    namespace
    {
        class ConsoleOutput
        {
            HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
            UINT previousCodePage = 0;
            DWORD previousMode = 0;
            bool restoreMode = false;
            bool ansiColor = false;

        public:
            explicit ConsoleOutput(bool useColor)
            {
                DWORD mode = 0;
                if (!GetConsoleMode(handle, &mode))
                    return;

                if (useColor)
                {
                    const DWORD ansiMode = mode | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                    ansiColor = ansiMode == mode || SetConsoleMode(handle, ansiMode);
                    restoreMode = ansiColor && ansiMode != mode;
                    previousMode = mode;
                }

                const UINT current = GetConsoleOutputCP();
                if (current != 0 && current != CP_UTF8)
                {
                    std::cout.flush();
                    if (SetConsoleOutputCP(CP_UTF8))
                        previousCodePage = current;
                }
            }

            bool supportsColor() const { return ansiColor; }

            ~ConsoleOutput()
            {
                if (restoreMode || previousCodePage != 0)
                    std::cout.flush();
                if (restoreMode)
                    SetConsoleMode(handle, previousMode);
                if (previousCodePage != 0)
                    SetConsoleOutputCP(previousCodePage);
            }
        };
    }
#endif

    void handleHelp()
    {
        std::cout << "Usage: allfetch [options]\n"
                  << "Options:\n"
                  << "  -h, --help       Show this help message\n"
                  << "  -v, --version    Show version information\n"
                  << "  -j, --json       Print system information as JSON\n"
                  << "  -a, --all        Show a more detailed system report\n"
                  << "      --no-logo    Hide the OS logo\n"
                  << "      --no-color   Disable colored output\n";
    }

    void handleVersion()
    {
        std::string version = "1.0.0";

        std::cout << "allfetch version " << version << "\n";
    }


    /* Implementation of the dispatchCommand function
    Input:
        result: The result of parsing the command line arguments
    Output:
        bool: true if the command was dispatched successfully, false otherwise
    */
    bool dispatchCommand(const allfetch::core::ParserResult &result)
    {
        switch (result.action)
        {
        case Action::help:
            handleHelp();
            return true;
        case Action::version:
            handleVersion();
            return true;
        case Action::fetch:
        {
#if defined(_WIN32)
            const ConsoleOutput consoleOutput(result.isColored && !result.isJson);
#endif
            try
            {
                const auto info = system::getSystemInfo();
                if (result.isJson)
                {
                    render::renderJson(info, std::cout);
                }
                else
                {
                    render::TerminalCapabilities terminal;
#if defined(_WIN32)
                    terminal.supportsAnsiColor = consoleOutput.supportsColor();
#elif defined(__linux__)
                    const char *term = std::getenv("TERM");
                    terminal.supportsAnsiColor = isatty(STDOUT_FILENO) &&
                        (!term || std::string_view(term) != "dumb");
#endif
                    const char *colorTerm = std::getenv("COLORTERM");
                    const std::string_view depth = colorTerm ? colorTerm : "";
                    terminal.supportsTrueColor = depth == "truecolor" || depth == "24bit";

                    render::PrettyOptions options{result.isColored, result.showLogo, result.showAll};
                    std::filesystem::path logoRoot;
                    render::LogoIndex index;
                    if (options.showLogo || (options.colorEnabled && terminal.supportsAnsiColor))
                    {
                        const auto &executable = info.titleInfo.exePath;
                        const std::filesystem::path executablePath(
                            std::u8string(executable.begin(), executable.end()));
                        if (executablePath.is_absolute())
                        {
                            logoRoot = executablePath.parent_path() / "logos";
                            try
                            {
                                index = render::LogoIndex::load(logoRoot / "logo_index.json");
                            }
                            catch (const std::runtime_error &)
                            {
                                options.showLogo = false;
                            }
                        }
                    }
                    render::prettyRender(info, index, logoRoot, options, terminal, std::cout);
                }
                return static_cast<bool>(std::cout);
            }
            catch (const std::exception &error)
            {
                std::cerr << "Error: " << error.what() << '\n';
                return false;
            }
        }
        default:
            return false;
        }
    }

}
