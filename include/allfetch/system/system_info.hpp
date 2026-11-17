#pragma once

#include <string>

namespace allfetch::system
{

    // Struct to hold the operating system information
    struct OSInfo
    {
        std::string name = "Unknown";
        std::string version = "Unknown";
        std::string kernelName = "Unknown";
        std::string kernelVersion = "Unknown";
        std::string architecture = "Unknown";
    };

    // Struct to hold the system information
    struct SystemInfo
    {
        OSInfo osInfo;
    };

    // Function to get the system information
    SystemInfo getSystemInfo();

}