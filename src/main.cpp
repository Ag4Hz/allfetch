#include <iostream>
#include "allfetch/core/argparser.hpp"
#include "allfetch/core/command_dispatch.hpp"

#if defined(_WIN32)
#include <limits>
#include <windows.h>

namespace
{
    bool showStandaloneConsoleHelp(int argc)
    {
        if (argc != 1)
            return false;

        const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
        const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        DWORD processId = 0;
        // Explorer usually starts Allfetch as the only process in a new console.
        if (!GetConsoleMode(input, &mode) || !GetConsoleMode(output, &mode) ||
            GetConsoleProcessList(&processId, 1) != 1)
            return false;

        std::cout << "To run allfetch, open Command Prompt in this folder and type:\n\n"
                     "  allfetch.exe\n\n"
                     "For more options, type allfetch.exe --help.\n\n"
                     "Press Enter to close. "
                  << std::flush;
        std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
        return true;
    }
}
#endif

int main(int argc, char *argv[])
{
#if defined(_WIN32)
    if (showStandaloneConsoleHelp(argc))
        return 0;
#endif

    allfetch::core::ParserResult result = allfetch::core::parseArguments(argc, argv);

    if (!result.errorMessage.empty())
    {
        std::cerr << "Error: " << result.errorMessage << std::endl;
        return 1;
    }

    if (!allfetch::core::dispatchCommand(result))
    {
        return 1;
    }

    return 0;
}
