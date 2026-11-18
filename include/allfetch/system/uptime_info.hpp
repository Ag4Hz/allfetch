#pragma once

#include "allfetch/system/system_info.hpp"

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <time.h>
#endif

namespace allfetch::system
{
    inline UptimeInfo getUptimeInfo()
    {
        UptimeInfo info;

#if defined(_WIN32)
        info.seconds = static_cast<long long>(
            GetTickCount64() / 1000ULL);
#elif defined(__linux__)
        struct timespec elapsed{};

        if (clock_gettime(CLOCK_BOOTTIME, &elapsed) == 0)
        {
            info.seconds = static_cast<long long>(elapsed.tv_sec);
        }
#endif

        return info;
    }
}