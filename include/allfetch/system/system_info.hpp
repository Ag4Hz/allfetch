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

    // Struct to hold disk/harddrive information
    struct DiskInfo
    {
        std::string filesystem = "Unknown";
        std::string mountpoint = "Unknown";
        std::string mountFrom = "Unknown";
        std::string type = "Unknown";
        std::string name = "";
        std::vector<std::string> volumeType;

        long long totalBytes = -1;
        long long freeBytes = -1;
        long long usedBytes = -1;

        long long totalFiles = -1;
        long long usedFiles = -1;

        long long creationTimeMs = -1;
    };

    // Struct to hold battery information
    struct BatteryInfo
    {
        std::string manufacturer = "Unknown";
        std::string manufactureDate = "Unknown";
        std::string modelName = "Unknown";
        std::string technology = "Unknown";
        std::string serial = "Unknown";
        std::vector<std::string> status;

        long long timeRemainingSeconds = -1;
        int cycleCount = -1;

        double capacityPercent = -1.0;
        double temperatureCelsius = -999.0;
    };

    // Struct for holding the locale
    struct LocaleInfo
    {
        std::string name = "Unknown";
    };

    // Struct for holding the color information
    struct ColorsInfo
    {
        std::vector<std::string> hexValues = std::vector<std::string>(16);
    };

    // Struct to hold information about the user environment
    struct TitleInfo
    {
        std::string userId = "Unknown";
        std::string userName = "Unknown";
        std::string fullUserName = "";
        std::string hostName = "Unknown";
        std::string homeDir = "Unknown";
        std::string exePath = "Unknown";
        std::string userShell = "";
        std::string cwd = "Unknown";

        long long pid = -1;
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
        std::vector<DiskInfo> diskInfo;
        std::vector<BatteryInfo> batteryInfo;
        LocaleInfo localeInfo;
        ColorsInfo colorsInfo;
        TitleInfo titleInfo;
    };

    // Function to get the system information
    SystemInfo getSystemInfo();
}
