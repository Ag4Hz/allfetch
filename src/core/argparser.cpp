#include "allfetch/core/argparser.hpp"
#include <cstring>

namespace allfetch::core
{

    /* Implementation of the parseArguments function
    Input:
        argc: The number of command line arguments
        argv: The array of command line arguments
    Output:
        ParserResult: The result of parsing the command line arguments
    */
    ParserResult parseArguments(int argc, char *argv[])
    {
        ParserResult result;
        std::string errorMessage = "";

        bool ActionSet = false;
        for (int i = 1; i < argc; i++)
        {
            if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
            {
                if (ActionSet)
                {
                    result.errorMessage = "Multiple actions specified";
                    return result;
                }
                result.action = Action::help;
                ActionSet = true;
            }
            else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0)
            {
                if (ActionSet)
                {
                    result.errorMessage = "Multiple actions specified";
                    return result;
                }
                result.action = Action::version;
                ActionSet = true;
            }
            else if (strcmp(argv[i], "--no-color") == 0)
            {
                result.isColored = false;
            }
            else
            {
                result.errorMessage = "Unknown argument: " + std::string(argv[i]) + "\nUse --help or -h for usage information.";
                return result;
            }
        }

        if (!ActionSet)
        {
            result.action = Action::fetch;
        }

        return result;
    }

}