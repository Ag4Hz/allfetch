#include "allfetch/system/windows/os_info.hpp"
#include "allfetch/system/system_info.hpp"
#include "allfetch/system/windows/memory_info.hpp"
#include "allfetch/system/windows/cpu_info.hpp"
#include "allfetch/system/windows/gpu_info.hpp"

namespace allfetch::system
{

    /* Implementation of the getSystemInfo function
    Output:
        SystemInfo: The system information
    */
    SystemInfo getSystemInfo()
    {
        SystemInfo info;

        // Get the operating system information
        Windows windows = getWindowsInfo();
        info.osInfo.name = windows.name;
        info.osInfo.version = windows.version;
        info.osInfo.kernelName = windows.kernelName;
        info.osInfo.kernelVersion = windows.kernelVersion;
        info.osInfo.architecture = windows.architecture;

        // Get the memory information
        MemoryInfo memoryInfo = getMemoryInfo();
        info.memoryInfo.totalPhysicalMemory = memoryInfo.totalPhysicalMemory;
        info.memoryInfo.availablePhysicalMemory = memoryInfo.availablePhysicalMemory;
        info.memoryInfo.totalVirtualMemory = memoryInfo.totalVirtualMemory;
        info.memoryInfo.availableVirtualMemory = memoryInfo.availableVirtualMemory;

        // Get the CPU information
        CPUInfo cpuInfo = getCPUInfo();
        info.cpuInfo.model = cpuInfo.model;
        info.cpuInfo.vendor = cpuInfo.vendor;
        info.cpuInfo.architecture = cpuInfo.architecture;
        info.cpuInfo.physicalCores = cpuInfo.physicalCores;
        info.cpuInfo.logicalCores = cpuInfo.logicalCores;
        info.cpuInfo.onlineCores = cpuInfo.onlineCores;
        info.cpuInfo.baseFrequency = cpuInfo.baseFrequency;
        info.cpuInfo.maxFrequency = cpuInfo.maxFrequency;

        // Get the GPU information
        std::vector<GPUInfo> gpuInfoList = getGPUInfo();
        info.gpuInfo = gpuInfoList;

        return info;
    }
}