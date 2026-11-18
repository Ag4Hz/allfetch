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

        std::cout << "Swap Total: "
                  << formatValue(info.swapInfo.totalBytes, " bytes") << '\n'
                  << "Swap Used: "
                  << formatValue(info.swapInfo.usedBytes, " bytes") << '\n';
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

        const auto &host = info.hostInfo;

        std::cout << "\nHost Family: " << host.family << '\n'
                  << "Host Name: " << host.name << '\n'
                  << "Host Version: " << host.version << '\n'
                  << "Host SKU: " << host.sku << '\n'
                  << "Host Vendor: " << host.vendor << '\n'
                  << "Host Serial: " << host.serial << '\n'
                  << "Host UUID: " << host.uuid << '\n';

        if (info.localIPInfo.empty())
        {
            std::cout << "Local IP: Unavailable\n";
        }

        for (const auto &ip : info.localIPInfo)
        {
            std::cout << "Local IP (" << ip.interfaceName << "): "
                      << ip.address << '/'
                      << ip.prefixLength << '\n';
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

        if (info.displayInfo.empty())
        {
            std::cout << "\nDisplay: Unavailable\n";
        }

        for (std::size_t index = 0;
             index < info.displayInfo.size();
             ++index)
        {
            const auto &display = info.displayInfo[index];

            std::cout << "\nDisplay " << index << ": "
                      << display.name << '\n'
                      << "Display ID: " << formatValue(display.id) << '\n'
                      << "Primary: "
                      << (display.primaryKnown
                              ? (display.primary ? "Yes" : "No")
                              : "Unknown")
                      << '\n'
                      << "Width: " << formatValue(display.width, " px") << '\n'
                      << "Height: " << formatValue(display.height, " px") << '\n'
                      << "Refresh Rate: "
                      << formatValue(display.refreshRateHz, " Hz") << '\n'
                      << "Effective DPI: "
                      << formatValue(display.effectiveDpi) << '\n'
                      << "Scaled Width: "
                      << formatValue(display.scaledWidth, " px") << '\n'
                      << "Scaled Height: "
                      << formatValue(display.scaledHeight, " px") << '\n'
                      << "Preferred Width: "
                      << formatValue(display.preferredWidth, " px") << '\n'
                      << "Preferred Height: "
                      << formatValue(display.preferredHeight, " px") << '\n'
                      << "Preferred Refresh Rate: "
                      << formatValue(display.preferredRefreshRateHz, " Hz") << '\n'
                      << "Physical Width: "
                      << formatValue(display.physicalWidthMm, " mm") << '\n'
                      << "Physical Height: "
                      << formatValue(display.physicalHeightMm, " mm") << '\n'
                      << "Rotation: "
                      << formatValue(display.rotationDegrees, " degrees") << '\n'
                      << "Bits Per Color Channel: "
                      << formatValue(display.bitsPerColorChannel) << '\n'
                      << "HDR: " << display.hdrStatus << '\n'
                      << "DRR: " << display.drrStatus << '\n'
                      << "Type: " << display.type << '\n'
                      << "Manufacture Year: "
                      << formatValue(display.manufactureYear) << '\n'
                      << "Manufacture Week: "
                      << formatValue(display.manufactureWeek) << '\n'
                      << "Serial: " << display.serial << '\n'
                      << "Platform API: " << display.platformApi << '\n';
        }

        if (info.batteryInfo.empty())
            std::cout << "\nBattery: Unavailable\n";

        for (const auto &battery : info.batteryInfo)
        {
            std::cout
                << "\nBattery: " << battery.modelName << '\n'
                << "Manufacturer: " << battery.manufacturer << '\n'
                << "Manufacture Date: " << battery.manufactureDate << '\n'
                << "Technology: " << battery.technology << '\n'
                << "Serial: " << battery.serial << '\n'
                << "Charge: "
                << formatValue(battery.capacityPercent, " %") << '\n'
                << "Cycle Count: "
                << formatValue(battery.cycleCount) << '\n'
                << "Time Remaining: "
                << formatValue(battery.timeRemainingSeconds, " s") << '\n';

            // Negative temperatures can be valid.
            std::cout << "Temperature: ";

            if (battery.temperatureCelsius == -1.0)
                std::cout << "Unknown\n";
            else
                std::cout << battery.temperatureCelsius << " C\n";

            std::cout << "Status:";

            if (battery.status.empty())
                std::cout << " Unknown";

            for (const auto &status : battery.status)
                std::cout << " [" << status << ']';

            std::cout << '\n';
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
