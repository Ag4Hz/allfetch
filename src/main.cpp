#include <iostream>
#include "allfetch/core/argparser.hpp"
#include "allfetch/core/command_dispatch.hpp"

int main(int argc, char *argv[])
{
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
