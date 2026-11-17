#include "allfetch/core/command_dispatch.hpp"
#include "allfetch/core/argparser.hpp"
#include "allfetch/system/system_info.hpp"
#include <iostream>

namespace allfetch::core
{

    void handleHelp()
    {
        std::cout << "Usage: allfetch [options]\n"
                  << "Options:\n"
                  << "  -h, --help       Show this help message\n"
                  << "  -v, --version    Show version information\n"
                  << "      --no-color   Disable colored output\n";
    }

    void handleVersion()
    {
        std::string version = "1.0.0";

        std::cout << "allfetch version " << version << "\n";
    }

    void handleFetch()
    {
        allfetch::system::SystemInfo info = allfetch::system::getSystemInfo();
        std::cout << "Operating System: " << info.osInfo.name << "\n"
                  << "Version: " << info.osInfo.version << "\n"
                  << "Kernel Name: " << info.osInfo.kernelName << "\n"
                  << "Kernel Version: " << info.osInfo.kernelVersion << "\n"
                  << "Architecture: " << info.osInfo.architecture << "\n";
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
            handleFetch();
            return true;
        default:
            return false;
        }
    }

}