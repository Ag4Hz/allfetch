#include "allfetch/core/command_dispatch.hpp"
#include "allfetch/core/argparser.hpp"
#include "allfetch/system/system_info.hpp"
#include <iostream>
#include <sstream>
#include <string>

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
        const auto info = allfetch::system::getSystemInfo();
        const auto formatValue = [](auto value, const char *unit = "") -> std::string
        {
            if (value < 0)
            {
                return "Unknown";
            }

            std::ostringstream text;
            text << value << unit;
            return text.str();
        };

        std::cout << "Operating System: " << info.osInfo.name << "\n"
                  << "Version: " << info.osInfo.version << "\n"
                  << "Kernel Name: " << info.osInfo.kernelName << "\n"
                  << "Kernel Version: " << info.osInfo.kernelVersion << "\n"
                  << "Architecture: " << info.osInfo.architecture << "\n"
                  << "\nTerminal: " << info.terminalInfo.name << "\n"
                  << "Terminal Version: " << info.terminalInfo.version << "\n"
                  << "Shell: " << info.shellInfo.name << "\n"
                  << "Shell Executable: " << info.shellInfo.executable << "\n"
                  << "Shell Version: " << info.shellInfo.version << "\n"
                  << "\nCPU: " << info.cpuInfo.model << "\n"
                  << "CPU Vendor: " << info.cpuInfo.vendor << "\n"
                  << "CPU Architecture: " << info.cpuInfo.architecture << "\n"
                  << "Physical Cores: " << formatValue(info.cpuInfo.physicalCores) << "\n"
                  << "Logical Cores: " << formatValue(info.cpuInfo.logicalCores) << "\n"
                  << "Online Cores: " << formatValue(info.cpuInfo.onlineCores) << "\n"
                  << "Base Frequency: " << formatValue(info.cpuInfo.baseFrequency, " MHz") << "\n"
                  << "Max Frequency: " << formatValue(info.cpuInfo.maxFrequency, " MHz") << "\n"
                  << "Reported Frequency: " << formatValue(info.cpuInfo.reportedFrequency, " MHz") << "\n"
                  << "\nPhysical Memory Total: " << formatValue(info.memoryInfo.totalPhysicalMemory, " bytes") << "\n"
                  << "Physical Memory Available: " << formatValue(info.memoryInfo.availablePhysicalMemory, " bytes") << "\n";

        const auto &memory = info.memoryInfo;
        const long long usedMemory =
            memory.totalPhysicalMemory >= 0 &&
                    memory.availablePhysicalMemory >= 0 &&
                    memory.availablePhysicalMemory <= memory.totalPhysicalMemory
                ? memory.totalPhysicalMemory - memory.availablePhysicalMemory
                : -1;

        std::cout << "Physical Memory Used: " << formatValue(usedMemory, " bytes") << "\n"
                  << "Process Virtual Memory Total: " << formatValue(memory.totalVirtualMemory, " bytes") << "\n"
                  << "Process Virtual Memory Available: " << formatValue(memory.availableVirtualMemory, " bytes") << "\n\n";

        const auto uptime = info.uptimeInfo.seconds;

        if (uptime < 0)
        {
            std::cout << "Uptime: Unknown\n";
        }
        else
        {
            const auto days = uptime / 86400;
            const auto hours = (uptime % 86400) / 3600;
            const auto minutes = (uptime % 3600) / 60;
            const auto seconds = uptime % 60;

            std::cout << "Uptime: "
                      << days << "d "
                      << hours << "h "
                      << minutes << "m "
                      << seconds << "s\n";
        }

        if (info.gpuInfo.empty())
        {
            std::cout << "\nGPU: Unavailable\n";
        }

        for (std::size_t index = 0; index < info.gpuInfo.size(); ++index)
        {
            const auto &gpu = info.gpuInfo[index];
            std::cout << "\nGPU " << index << ": " << gpu.model << "\n"
                      << "GPU Vendor: " << gpu.vendor << "\n"
                      << "GPU Driver: " << gpu.driverVersion << "\n"
                      << "GPU Type: " << gpu.type << "\n"
                      << "GPU Device ID: " << formatValue(gpu.deviceId) << "\n"
                      << "Dedicated GPU Memory: " << formatValue(gpu.memorySizeMB, " MiB") << "\n"
                      << "Shared GPU Memory Limit: " << formatValue(gpu.sharedMemorySizeMB, " MiB") << "\n";
        }
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
