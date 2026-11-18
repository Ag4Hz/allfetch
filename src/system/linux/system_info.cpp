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

        return info;
    }

}