#pragma once

#include <allfetch/system/system_info.hpp>
#include <windows.h>
#include "allfetch/system/windows/platform.hpp"
#include <string>
#include <memory>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <cstring>

#if defined(_MSC_VER) || defined(_M_IX86) || defined(_M_X64)
#include <intrin.h>
#elif defined(__i386__) || defined(__x86_64__)
#include <cpuid.h>
#endif

namespace allfetch::system
{
    namespace cpu_detail
    {
        inline void collectSmbiosMaximumFrequency(CPUInfo &info)
        {
            if (info.maxFrequency > 0)
            {
                return;
            }

            // Numeric form of the Windows 'RSMB' provider signature.
            constexpr DWORD provider = 0x52534D42;
            constexpr std::size_t wrapperSize = 8;

            const UINT required = GetSystemFirmwareTable(
                provider, 0, nullptr, 0);

            if (required < wrapperSize)
            {
                return;
            }

            std::vector<unsigned char> buffer(required);

            const UINT received = GetSystemFirmwareTable(
                provider, 0, buffer.data(), required);

            if (received < wrapperSize || received > buffer.size())
            {
                return;
            }

            DWORD tableLength = 0;
            std::memcpy(&tableLength, buffer.data() + 4, sizeof(tableLength));

            if (tableLength > received - wrapperSize)
            {
                return;
            }

            const auto *table = buffer.data() + wrapperSize;
            const std::size_t end = tableLength;

            std::size_t position = 0;
            unsigned int maximum = 0;

            while (position < end && end - position >= 4)
            {
                const auto *record = table + position;
                const unsigned int type = record[0];
                const std::size_t length = record[1];

                if (length < 4 || length > end - position)
                {
                    return;
                }

                if (type == 127)
                {
                    break;
                }

                // Type 4: Processor Information.
                if (type == 4 && length >= 0x19)
                {
                    const bool centralProcessor = record[0x05] == 0x03;
                    const bool populated = (record[0x18] & 0x40) != 0;
                    const bool enabled = (record[0x18] & 0x07) == 1;

                    if (centralProcessor && populated && enabled)
                    {
                        const unsigned int speed =
                            static_cast<unsigned int>(record[0x14]) |
                            (static_cast<unsigned int>(record[0x15]) << 8);

                        // Apply a plausibility check similar to Fastfetch.
                        const bool plausible =
                            speed != 0 &&
                            (info.baseFrequency <= 0 ||
                             (speed >= info.baseFrequency &&
                              speed <= info.baseFrequency * 2));

                        if (plausible && speed > maximum)
                        {
                            maximum = speed;
                        }
                    }
                }

                // Each formatted record is followed by strings,
                // terminated by two consecutive null bytes.
                std::size_t next = position + length;

                while (next < end && end - next >= 2 &&
                       !(table[next] == 0 && table[next + 1] == 0))
                {
                    ++next;
                }

                if (next >= end || end - next < 2)
                {
                    return;
                }

                position = next + 2;
            }

            if (maximum != 0)
            {
                info.maxFrequency = static_cast<float>(maximum);
            }
        }

        inline void collectRegistryFrequency(CPUInfo &info)
        {
            DWORD frequency = 0;
            DWORD bytes = sizeof(frequency);

            const auto status = RegGetValueW(
                HKEY_LOCAL_MACHINE,
                L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                L"~MHz",
                RRF_RT_REG_DWORD,
                nullptr,
                &frequency,
                &bytes);

            if (status == ERROR_SUCCESS &&
                bytes == sizeof(frequency) &&
                frequency != 0)
            {
                info.reportedFrequency = static_cast<float>(frequency);
            }
        }

        inline std::string readRegistryString(const wchar_t *valueName)
        {
            constexpr wchar_t key[] =
                L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";

            wchar_t buffer[512]{};
            DWORD bytes = sizeof(buffer);

            if (RegGetValueW(
                    HKEY_LOCAL_MACHINE,
                    key,
                    valueName,
                    RRF_RT_REG_SZ,
                    nullptr,
                    buffer,
                    &bytes) != ERROR_SUCCESS)
            {
                return "Unknown";
            }

            const int required = WideCharToMultiByte(
                CP_UTF8, 0, buffer, -1, nullptr, 0, nullptr, nullptr);

            if (required <= 1)
            {
                return "Unknown";
            }

            // Include space for the terminating null character.
            std::string text(required, '\0');

            if (WideCharToMultiByte(
                    CP_UTF8, 0, buffer, -1,
                    text.data(), required, nullptr, nullptr) == 0)
            {
                return "Unknown";
            }

            text.pop_back();

            const auto first = text.find_first_not_of(" \t\r\n");
            if (first == std::string::npos)
            {
                return "Unknown";
            }

            const auto last = text.find_last_not_of(" \t\r\n");
            return text.substr(first, last - first + 1);
        }

        inline void collectFrequencies(CPUInfo &info)
        {
            unsigned int base = 0;
            unsigned int maximum = 0;

#if defined(_M_IX86) || defined(_M_X64)
            int registers[4]{};
            __cpuidex(registers, 0, 0);

            if (static_cast<unsigned int>(registers[0]) < 0x16)
            {
                return;
            }

            __cpuidex(registers, 0x16, 0);
            base = static_cast<unsigned int>(registers[0]) & 0xFFFFu;
            maximum = static_cast<unsigned int>(registers[1]) & 0xFFFFu;

#elif defined(__i386__) || defined(__x86_64__)
            if (__get_cpuid_max(0, nullptr) < 0x16)
            {
                return;
            }

            unsigned int eax{}, ebx{}, ecx{}, edx{};
            __cpuid_count(0x16, 0, eax, ebx, ecx, edx);

            base = eax & 0xFFFFu;
            maximum = ebx & 0xFFFFu;
#endif

            if (base != 0)
            {
                info.baseFrequency = static_cast<float>(base);
            }

            if (maximum != 0)
            {
                info.maxFrequency = static_cast<float>(maximum);
            }
        }

        inline int countPhysicalCores()
        {
            DWORD bytes = 0;

            GetLogicalProcessorInformationEx(
                RelationProcessorCore, nullptr, &bytes);

            if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || bytes == 0)
            {
                return -1;
            }

            // This storage provides sufficient alignment for API records.
            const std::size_t units =
                (bytes + sizeof(std::max_align_t) - 1) /
                sizeof(std::max_align_t);

            std::vector<std::max_align_t> storage(units);

            auto *records =
                reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *>(
                    storage.data());

            if (!GetLogicalProcessorInformationEx(
                    RelationProcessorCore, records, &bytes))
            {
                return -1;
            }

            DWORD offset = 0;
            int count = 0;

            while (offset < bytes)
            {
                const auto *record =
                    reinterpret_cast<
                        const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *>(
                        reinterpret_cast<const unsigned char *>(
                            storage.data()) +
                        offset);

                if (record->Size == 0 ||
                    record->Size > bytes - offset)
                {
                    return -1;
                }

                if (record->Relationship == RelationProcessorCore)
                {
                    ++count;
                }

                offset += record->Size;
            }

            return count;
        }
    }

    /* Function to get the CPU information of the system
    Output:
        CPUInfo: The CPU information of the system
    */
    inline CPUInfo getCPUInfo()
    {
        CPUInfo cpuInfo;

        cpuInfo.model = cpu_detail::readRegistryString(L"ProcessorNameString");
        cpuInfo.vendor = cpu_detail::readRegistryString(L"VendorIdentifier");

        cpuInfo.architecture = windows_platform::architecture();

        cpuInfo.physicalCores = cpu_detail::countPhysicalCores();

        const DWORD active = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);

        if (active)
        {
            cpuInfo.logicalCores = static_cast<int>(active);
            cpuInfo.onlineCores = static_cast<int>(active);
        }

        cpu_detail::collectRegistryFrequency(cpuInfo);
        cpu_detail::collectFrequencies(cpuInfo);
        cpu_detail::collectSmbiosMaximumFrequency(cpuInfo);

        return cpuInfo;
    }

}