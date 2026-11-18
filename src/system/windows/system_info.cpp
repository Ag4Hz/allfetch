#include "allfetch/system/windows/os_info.hpp"
#include "allfetch/system/system_info.hpp"

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

        return info;
    }
}