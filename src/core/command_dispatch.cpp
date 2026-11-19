#include "allfetch/core/command_dispatch.hpp"
#include "allfetch/core/argparser.hpp"
#include "allfetch/render/render.hpp"
#include "allfetch/system/system_info.hpp"
#include <exception>
#include <iostream>
#include <algorithm>
#include <charconv>
#include <sstream>
#include <string>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace allfetch::core
{
#if defined(_WIN32)
    namespace
    {
        class Utf8ConsoleOutput
        {
            UINT previousCodePage = 0;

        public:
            Utf8ConsoleOutput()
            {
                DWORD mode = 0;
                if (!GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &mode))
                    return;

                const UINT current = GetConsoleOutputCP();
                if (current != 0 && current != CP_UTF8)
                {
                    std::cout.flush();
                    if (SetConsoleOutputCP(CP_UTF8))
                        previousCodePage = current;
                }
            }

            ~Utf8ConsoleOutput()
            {
                if (previousCodePage != 0)
                {
                    std::cout.flush();
                    SetConsoleOutputCP(previousCodePage);
                }
            }
        };
    }
#endif

    void handleHelp()
    {
        std::cout << "Usage: allfetch [options]\n"
                  << "Options:\n"
                  << "  -h, --help       Show this help message\n"
                  << "  -v, --version    Show version information\n"
                  << "  -j, --json       Print system information as JSON\n"
                  << "      --no-color   Disable colored output\n";
    }

    void handleVersion()
    {
        std::string version = "1.0.0";

        std::cout << "allfetch version " << version << "\n";
    }

    void printColorPalette(
        const allfetch::system::ColorsInfo &colors,
        bool useColor,
        bool supportsTrueColor)
    {
        if (!useColor)
            return;

        const std::size_t count =
            std::min<std::size_t>(colors.hexValues.size(), 16);

        for (std::size_t index = 0; index < count; ++index)
        {
            const auto &hex = colors.hexValues[index];

            unsigned rgb = 0;
            bool validHex = false;

            if (hex.size() == 7 && hex.front() == '#')
            {
                const auto result = std::from_chars(
                    hex.data() + 1,
                    hex.data() + hex.size(),
                    rgb,
                    16);

                validHex =
                    result.ec == std::errc{} && result.ptr == hex.data() + hex.size();
            }

            if (supportsTrueColor && validHex)
            {
                std::cout
                    << "\x1b[48;2;"
                    << ((rgb >> 16) & 255) << ';'
                    << ((rgb >> 8) & 255) << ';'
                    << (rgb & 255) << 'm';
            }
            else
            {
                const int background =
                    index < 8
                        ? 40 + static_cast<int>(index)
                        : 100 + static_cast<int>(index - 8);

                std::cout << "\x1b[" << background << 'm';
            }

            std::cout << "  ";

            if ((index + 1) % 8 == 0 || index + 1 == count)
                std::cout << "\x1b[0m\n";
        }
    }

    void handleFetch()
    {
#if defined(_WIN32)
        const Utf8ConsoleOutput consoleOutput;
#endif
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

        if (info.diskInfo.empty())
            std::cout << "\nDisk: Unavailable\n";

        for (const auto &disk : info.diskInfo)
        {
            std::cout
                << "\nDisk: " << disk.mountpoint << '\n'
                << "Name: " << disk.name << '\n'
                << "Filesystem: " << disk.filesystem << '\n'
                << "Mounted From: " << disk.mountFrom << '\n'
                << "Storage Medium: " << disk.type << '\n'
                << "Total: " << formatValue(disk.totalBytes, " bytes") << '\n'
                << "Free: " << formatValue(disk.freeBytes, " bytes") << '\n'
                << "Used: " << formatValue(disk.usedBytes, " bytes") << '\n'
                << "Total Files/Inodes: " << formatValue(disk.totalFiles) << '\n'
                << "Used Files/Inodes: " << formatValue(disk.usedFiles) << '\n'
                << "Creation Time: "
                << formatValue(disk.creationTimeMs, " ms since Unix epoch")
                << '\n'
                << "Volume Type:";

            for (const auto &type : disk.volumeType)
                std::cout << " [" << type << ']';

            std::cout << '\n';
        }

        std::cout << "Locale: " << info.localeInfo.name << '\n';

        const auto &title = info.titleInfo;

        std::cout
            << "\nTitle: " << title.userName << '@' << title.hostName << '\n'
            << "User ID: " << title.userId << '\n'
            << "User Name: " << title.userName << '\n'
            << "Full User Name: " << title.fullUserName << '\n'
            << "Host Name: " << title.hostName << '\n'
            << "Home Directory: " << title.homeDir << '\n'
            << "Executable Path: " << title.exePath << '\n'
            << "Account Shell: " << title.userShell << '\n'
            << "Process ID: " << formatValue(title.pid) << '\n'
            << "Working Directory: " << title.cwd << '\n';

        std::cout
            << "\nWindow Manager: "
            << info.windowManagerInfo.name << '\n'
            << "Window Manager Theme: "
            << info.windowManagerInfo.theme << '\n';

        const auto &appearance = info.desktopAppearanceInfo;

        std::cout
            << "\nDesktop Theme: " << appearance.theme << '\n'
            << "Desktop Color Scheme: " << appearance.colorScheme << '\n'
            << "Icon Theme: " << appearance.iconTheme << '\n'
            << "Interface Font: " << appearance.font << '\n'
            << "Cursor Theme: " << appearance.cursorTheme << '\n'
            << "Cursor Size: " << formatValue(appearance.cursorSize, " px (nominal)") << '\n';

#if defined(_WIN32)
        std::cout << "Desktop Icons: ";
        if (!appearance.desktopIconsKnown)
            std::cout << "Unknown";
        else if (appearance.desktopIcons.empty())
            std::cout << "None";
        else
        {
            for (std::size_t index = 0; index < appearance.desktopIcons.size(); ++index)
            {
                if (index != 0)
                    std::cout << ", ";
                std::cout << appearance.desktopIcons[index];
            }
        }
        std::cout << '\n';
#endif

        std::cout
            << "Terminal Font: " << info.terminalInfo.font << '\n'
            << "Terminal Font Source: "
            << info.terminalInfo.fontSource << '\n';
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
            if (result.isJson)
            {
                try
                {
                    const auto info = allfetch::system::getSystemInfo();
                    allfetch::render::renderJson(info, std::cout);
                    return static_cast<bool>(std::cout);
                }
                catch (const std::exception &error)
                {
                    std::cerr << "Error: " << error.what() << '\n';
                    return false;
                }
            }
            handleFetch();
            return true;
        default:
            return false;
        }
    }

}
