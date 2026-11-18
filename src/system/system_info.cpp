#if defined(_WIN32)
#include <winsock2.h>
#include "allfetch/system/windows/os_info.hpp"
#include "allfetch/system/windows/memory_info.hpp"
#include "allfetch/system/windows/gpu_info.hpp"
#include "allfetch/system/windows/cpu_info.hpp"
#elif defined(__linux__)
#include "allfetch/system/linux/os_info.hpp"
#include "allfetch/system/linux/memory_info.hpp"
#include "allfetch/system/linux/gpu_info.hpp"
#include "allfetch/system/linux/cpu_info.hpp"
#endif

#include "allfetch/system/system_info.hpp"
#include "allfetch/system/shell_info.hpp"
#include "allfetch/system/terminal_info.hpp"
#include "allfetch/system/uptime_info.hpp"
#include "allfetch/system/swap_info.hpp"
#include "allfetch/system/local_ip_info.hpp"

namespace allfetch::system
{

    /* Implementation of the getSystemInfo function
    Output:
        SystemInfo: The system information
    */
    SystemInfo getSystemInfo()
    {
        SystemInfo info;

        info.osInfo = getOSInfo();
        info.memoryInfo = getMemoryInfo();
        info.cpuInfo = getCPUInfo();
        info.gpuInfo = getGPUInfo();
        info.terminalInfo = getTerminalInfo();
        info.shellInfo = getShellInfo();
        info.uptimeInfo = getUptimeInfo();
        info.swapInfo = getSwapInfo();
        info.localIPInfo = getLocalIPInfo();

        return info;
    }
}
