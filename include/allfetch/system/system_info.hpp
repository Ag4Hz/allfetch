#pragma once

#include <string>

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

    // Struct to hold the system information
    struct SystemInfo
    {
        OSInfo osInfo;
        MemoryInfo memoryInfo;
        CPUInfo cpuInfo;
        GPUInfo gpuInfo;
    };

    // Function to get the system information
    SystemInfo getSystemInfo();

}