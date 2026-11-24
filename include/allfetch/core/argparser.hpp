#pragma once

#include <string>

namespace allfetch::core
{

    // All possible actions
    enum class Action
    {
        help,
        version,
        fetch,
    };

    enum class ColorMode
    {
        automatic,
        always,
        never,
    };

    // Struct to hold the result of parsing the command line arguments
    struct ParserResult
    {
        Action action = Action::fetch;
        ColorMode colorMode = ColorMode::automatic;
        bool isJson = false;
        std::string errorMessage;
        bool showLogo = true;
        bool showAll = false;
    };

    // Function to call in main for parsing the command line arguments
    ParserResult parseArguments(int argc, char *argv[]);

}
