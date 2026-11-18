#pragma once

#include "allfetch/system/system_info.hpp"
#include <windows.h>

namespace allfetch::system
{

    /* Function to get the memory information of the system
    Output:
        MemoryInfo: The memory information of the system
    */
    inline MemoryInfo getMemoryInfo()
    {
        MemoryInfo memoryInfo;

        MEMORYSTATUSEX statex;
        statex.dwLength = sizeof(statex);

        if (!GlobalMemoryStatusEx(&statex))
        {
            return memoryInfo;
        }

        memoryInfo.totalPhysicalMemory = static_cast<long long>(statex.ullTotalPhys);
        memoryInfo.availablePhysicalMemory = static_cast<long long>(statex.ullAvailPhys);
        memoryInfo.totalVirtualMemory = static_cast<long long>(statex.ullTotalVirtual);
        memoryInfo.availableVirtualMemory = static_cast<long long>(statex.ullAvailVirtual);

        return memoryInfo;
    }

}