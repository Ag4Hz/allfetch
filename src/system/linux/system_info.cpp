#include "allfetch/system/linux/os_info.hpp"
#include "allfetch/system/system_info.hpp"
#include "allfetch/system/linux/memory_info.hpp"
#include "allfetch/system/linux/cpu_info.hpp"
#include "allfetch/system/linux/gpu_info.hpp"

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
        Linux linux = getLinuxInfo();
        info.osInfo.name = linux.name;
        info.osInfo.version = linux.version;
        info.osInfo.kernelName = linux.kernelName;
        info.osInfo.kernelVersion = linux.kernelVersion;
        info.osInfo.architecture = linux.architecture;

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