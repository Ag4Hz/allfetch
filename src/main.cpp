#include <iostream>
#include "allfetch/core/argparser.hpp"

int main(int argc, char *argv[])
{
    allfetch::core::ParserResult result = allfetch::core::parseArguments(argc, argv);

    if (!result.errorMessage.empty())
    {
        std::cerr << "Error: " << result.errorMessage << std::endl;
        return 1;
    }

    std::cout << "Action: ";
    std::cout << (result.action == allfetch::core::Action::help ? "help" : result.action == allfetch::core::Action::version ? "version"
                                                                                                                            : "fetch")
              << std::endl;
    std::cout << "Colored output: " << (result.isColored ? "enabled" : "disabled") << std::endl;

    return 0;
}
