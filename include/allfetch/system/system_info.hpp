#pragma once

#include <string>
#include <vector>

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

    // Struct to hold terminal information
    struct TerminalInfo
    {
        std::string name = "Unknown";
        std::string version = "Unknown";
    };

    // Struct to hold shell information
    struct ShellInfo
    {
        std::string name = "Unknown";
        std::string executable = "Unknown";
        std::string version = "Unknown";
    };

    // Struct to hold uptime information
    struct UptimeInfo
    {
        long long seconds = -1;
    };

    // Struct to hold the memory information
    struct MemoryInfo
    {
        long long totalPhysicalMemory = -1;
        long long availablePhysicalMemory = -1;
        long long totalVirtualMemory = -1;
        long long availableVirtualMemory = -1;
    };

    // Struct to hold the CPU information
    struct CPUInfo
    {
        std::string model = "Unknown";
        std::string vendor = "Unknown";
        std::string architecture = "Unknown";
        int physicalCores = -1;
        int logicalCores = -1;
        int onlineCores = -1;
        float reportedFrequency = -1.0f;
        float baseFrequency = -1.0f;
        float maxFrequency = -1.0f;
    };

    // Struct to hold GPU information
    struct GPUInfo
    {
        std::string model = "Unknown";
        std::string vendor = "Unknown";
        std::string driverVersion = "Unknown";
        std::string type = "Unknown"; // Integrated or Discrete
        int deviceId = -1;
        int memorySizeMB = -1;
        int sharedMemorySizeMB = -1;
    };

    // Struct to hold swap information
    struct SwapInfo
    {
        long long totalBytes = -1;
        long long usedBytes = -1;
    };

    // Struct to hold local IP information
    struct LocalIPInfo
    {
        std::string interfaceName = "Unknown";
        std::string address = "Unknown";
        int prefixLength = -1;
    };

    // Struct to hold host information
    struct HostInfo
    {
        std::string family = "Unknown";
        std::string name = "Unknown";
        std::string version = "Unknown";
        std::string sku = "Unknown";
        std::string vendor = "Unknown";
        std::string serial = "Unknown";
        std::string uuid = "Unknown";
    };

    // Struct to hold display information
    struct DisplayInfo
    {
        std::string name = "Unknown";
        std::string drrStatus = "Unknown";
        std::string hdrStatus = "Unknown";
        std::string type = "Unknown";
        std::string serial = "Unknown";
        std::string platformApi = "Unknown";

        bool primary = false;
        bool primaryKnown = false;
        long long id = -1;

        int width = -1;
        int height = -1;

        int effectiveDpi = -1;
        int scaledWidth = -1;
        int scaledHeight = -1;

        int preferredWidth = -1;
        int preferredHeight = -1;

        int physicalWidthMm = -1;
        int physicalHeightMm = -1;

        int rotationDegrees = -1;
        int bitsPerColorChannel = -1;

        int manufactureYear = -1;
        int manufactureWeek = -1;

        double refreshRateHz = -1.0;
        double preferredRefreshRateHz = -1.0;
    };

    // Struct to hold the system information
    struct SystemInfo
    {
        OSInfo osInfo;
        HostInfo hostInfo;
        MemoryInfo memoryInfo;
        CPUInfo cpuInfo;
        std::vector<GPUInfo> gpuInfo;
        TerminalInfo terminalInfo;
        ShellInfo shellInfo;
        UptimeInfo uptimeInfo;
        SwapInfo swapInfo;
        std::vector<LocalIPInfo> localIPInfo;
        std::vector<DisplayInfo> displayInfo;
    };

    // Function to get the system information
    SystemInfo getSystemInfo();
}
