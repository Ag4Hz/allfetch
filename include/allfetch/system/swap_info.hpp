#pragma once

#include "allfetch/system/system_info.hpp"

#include <cstdint>
#include <limits>

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#elif defined(__linux__)
#include <sys/sysinfo.h>
#endif

namespace allfetch::system
{
#if defined(_WIN32)
    namespace swap_detail
    {
        struct PageTotals
        {
            std::uint64_t total = 0;
            std::uint64_t used = 0;
            bool valid = true;
        };

        inline BOOL CALLBACK collectPageFile(
            LPVOID context,
            PENUM_PAGE_FILE_INFORMATION pageFile,
            LPCWSTR)
        {
            auto &totals = *static_cast<PageTotals *>(context);

            constexpr auto maximum =
                std::numeric_limits<std::uint64_t>::max();

            if (pageFile->TotalInUse > pageFile->TotalSize ||
                pageFile->TotalSize > maximum - totals.total ||
                pageFile->TotalInUse > maximum - totals.used)
            {
                totals.valid = false;
                return FALSE;
            }

            totals.total += pageFile->TotalSize;
            totals.used += pageFile->TotalInUse;

            return TRUE;
        }
    }
#endif

    inline SwapInfo getSwapInfo()
    {
        SwapInfo info;

#if defined(_WIN32)
        swap_detail::PageTotals totals;

        if (!EnumPageFilesW(swap_detail::collectPageFile, &totals) ||
            !totals.valid)
        {
            return info;
        }

        SYSTEM_INFO system{};
        GetSystemInfo(&system);

        const auto pageSize = system.dwPageSize;
        const auto maximum = static_cast<std::uint64_t>(
            std::numeric_limits<long long>::max());

        if (pageSize == 0 || totals.total > maximum / pageSize)
        {
            return info;
        }

        info.totalBytes = static_cast<long long>(
            totals.total * pageSize);

        info.usedBytes = static_cast<long long>(
            totals.used * pageSize);

#elif defined(__linux__)
        struct sysinfo status{};

        if (sysinfo(&status) != 0 ||
            status.mem_unit == 0 ||
            status.freeswap > status.totalswap)
        {
            return info;
        }

        const auto maximum = static_cast<std::uint64_t>(
            std::numeric_limits<long long>::max());

        if (status.totalswap > maximum / status.mem_unit)
        {
            return info;
        }

        info.totalBytes = static_cast<long long>(
            static_cast<std::uint64_t>(status.totalswap) *
            status.mem_unit);

        info.usedBytes = static_cast<long long>(
            static_cast<std::uint64_t>(
                status.totalswap - status.freeswap) *
            status.mem_unit);
#endif

        return info;
    }
}