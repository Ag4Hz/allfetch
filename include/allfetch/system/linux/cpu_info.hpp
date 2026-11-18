#pragma once

#include "allfetch/system/system_info.hpp"

#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <string>
#include <system_error>
#include <tuple>

#if defined(__i386__) || defined(__x86_64__)
#include <cpuid.h>
#endif

namespace allfetch::system
{
    namespace linux_cpu_detail
    {
        namespace fs = std::filesystem;

        inline std::optional<long long> readNumber(const fs::path &path)
        {
            std::ifstream file(path);
            long long value = 0;

            if (!(file >> value) || value < 0)
            {
                return std::nullopt;
            }

            return value;
        }

        inline std::string trim(const std::string &text)
        {
            const auto first = text.find_first_not_of(" \t\r\n");

            if (first == std::string::npos)
            {
                return {};
            }

            const auto last = text.find_last_not_of(" \t\r\n");
            return text.substr(first, last - first + 1);
        }

        inline bool isCpuDirectory(const std::string &name)
        {
            return name.size() > 3 &&
                   name.starts_with("cpu") &&
                   std::all_of(
                       name.begin() + 3,
                       name.end(),
                       [](char character)
                       {
                           return character >= '0' && character <= '9';
                       });
        }

        inline void collectIdentity(CPUInfo &info)
        {
            std::ifstream file("/proc/cpuinfo");
            std::string line;

            std::optional<unsigned int> implementer;
            std::optional<unsigned int> part;

            const auto parseHex = [](const std::string &text)
                -> std::optional<unsigned int>
            {
                std::istringstream input(text);
                unsigned int value = 0;

                if (input >> std::hex >> value)
                {
                    return value;
                }

                return std::nullopt;
            };

            while (std::getline(file, line))
            {
                const auto separator = line.find(':');

                if (separator == std::string::npos)
                {
                    continue;
                }

                const auto key = trim(line.substr(0, separator));
                const auto value = trim(line.substr(separator + 1));

                if (value.empty())
                {
                    continue;
                }

                if (key == "model name" && info.model == "Unknown")
                {
                    info.model = value;
                }
                else if (key == "vendor_id" && info.vendor == "Unknown")
                {
                    info.vendor = value;
                }
                else if (key == "CPU implementer" && !implementer)
                {
                    implementer = parseHex(value);
                }
                else if (key == "CPU part" && !part)
                {
                    part = parseHex(value);
                }
                else if (key == "cpu MHz")
                {
                    std::istringstream input(value);
                    float mhz = 0;

                    if (input >> mhz && mhz > info.reportedFrequency)
                    {
                        info.reportedFrequency = mhz;
                    }
                }
            }

            if (implementer && *implementer == 0x41)
            {
                if (info.vendor == "Unknown")
                {
                    info.vendor = "Arm";
                }

                if (part && info.model == "Unknown")
                {
                    switch (*part)
                    {
                    case 0xd03:
                        info.model = "Arm Cortex-A53";
                        break;
                    case 0xd08:
                        info.model = "Arm Cortex-A72";
                        break;
                    case 0xd0b:
                        info.model = "Arm Cortex-A76";
                        break;
                    }
                }
            }

            struct utsname system{};

            if (uname(&system) == 0)
            {
                info.architecture = system.machine;
            }
        }

        inline void collectTopology(CPUInfo &info)
        {
            const int configured = get_nprocs_conf();
            const int online = get_nprocs();

            if (configured > 0)
            {
                info.logicalCores = configured;
            }

            if (online > 0)
            {
                info.onlineCores = online;
            }

            std::set<std::tuple<long long, long long, long long>> cores;
            bool complete = true;

            std::error_code error;
            fs::directory_iterator entry(
                "/sys/devices/system/cpu", error);
            const fs::directory_iterator end;

            if (error)
            {
                return;
            }

            while (entry != end)
            {
                const auto path = entry->path();
                const auto name = path.filename().string();

                if (isCpuDirectory(name))
                {
                    const auto topology = path / "topology";
                    const auto package = readNumber(
                        topology / "physical_package_id");
                    const auto core = readNumber(
                        topology / "core_id");

                    // Not all architectures expose a die ID.
                    const auto die = readNumber(
                                         topology / "die_id")
                                         .value_or(0);

                    if (package && core)
                    {
                        cores.emplace(*package, die, *core);
                    }
                    else
                    {
                        complete = false;
                    }
                }

                entry.increment(error);

                if (error)
                {
                    return;
                }
            }

            if (complete && !cores.empty())
            {
                info.physicalCores = static_cast<int>(cores.size());
            }
        }

        inline void collectFrequencies(CPUInfo &info)
        {
            std::error_code error;
            fs::directory_iterator entry(
                "/sys/devices/system/cpu/cpufreq", error);
            const fs::directory_iterator end;

            while (!error && entry != end)
            {
                const auto path = entry->path();

                if (path.filename().string().starts_with("policy"))
                {
                    if (const auto base =
                            readNumber(path / "base_frequency");
                        base && *base > 0)
                    {
                        const float mhz =
                            static_cast<float>(*base) / 1000.0f;

                        if (mhz > info.baseFrequency)
                        {
                            info.baseFrequency = mhz;
                        }
                    }

                    auto maximum = readNumber(
                        path / "cpuinfo_max_freq");

                    if (!maximum || *maximum == 0)
                    {
                        maximum = readNumber(
                            path / "scaling_max_freq");
                    }

                    if (maximum && *maximum > 0)
                    {
                        const float mhz =
                            static_cast<float>(*maximum) / 1000.0f;

                        if (mhz > info.maxFrequency)
                        {
                            info.maxFrequency = mhz;
                        }
                    }
                }

                entry.increment(error);
            }

#if defined(__i386__) || defined(__x86_64__)
            // Fill missing values from CPUID when supported.
            if (__get_cpuid_max(0, nullptr) >= 0x16)
            {
                unsigned int eax{}, ebx{}, ecx{}, edx{};
                __cpuid_count(0x16, 0, eax, ebx, ecx, edx);

                const unsigned int base = eax & 0xFFFFu;
                const unsigned int maximum = ebx & 0xFFFFu;

                if (info.baseFrequency <= 0 && base != 0)
                {
                    info.baseFrequency = static_cast<float>(base);
                }

                if (info.maxFrequency <= 0 && maximum != 0)
                {
                    info.maxFrequency = static_cast<float>(maximum);
                }
            }
#endif
        }
    }

    /* Function to get the CPU information of the system
    Output:
        CPUInfo: The CPU information of the system
    */
    inline CPUInfo getCPUInfo()
    {
        CPUInfo cpuInfo;

        linux_cpu_detail::collectIdentity(cpuInfo);
        linux_cpu_detail::collectTopology(cpuInfo);
        linux_cpu_detail::collectFrequencies(cpuInfo);

        return cpuInfo;
    }
}