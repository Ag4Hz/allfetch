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

    // Struct to hold the result of parsing the command line arguments
    struct ParserResult
    {
        Action action = Action::fetch;
        bool isColored = true;
        bool isJson = false;
        std::string errorMessage;
    };

    // Function to call in main for parsing the command line arguments
    ParserResult parseArguments(int argc, char *argv[]);

}
