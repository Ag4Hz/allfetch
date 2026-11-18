#pragma once

#include "allfetch/system/system_info.hpp"
#include <fstream>
#include <limits>
#include <sstream>
#include <string>

namespace allfetch::system
{
    /* Function to get the memory information of the system
    Output:
        MemoryInfo: The memory information of the system
    */
    inline MemoryInfo getMemoryInfo()
    {
        MemoryInfo memoryInfo;
        std::ifstream file("/proc/meminfo");
        std::string line;

        while (std::getline(file, line))
        {
            std::istringstream fields(line);

            std::string key;
            std::string unit;
            long long value = 0;

            if (!(fields >> key >> value >> unit))
            {
                continue;
            }

            if (unit != "kB" || value < 0 ||
                value > std::numeric_limits<long long>::max() / 1024)
            {
                continue;
            }

            if (key == "MemTotal:")
            {
                memoryInfo.totalPhysicalMemory = value * 1024;
            }
            else if (key == "MemAvailable:")
            {
                memoryInfo.availablePhysicalMemory = value * 1024;
            }
        }

        return memoryInfo;
    }
}