#include "allfetch/system/linux/os_info.hpp"
#include "allfetch/system/system_info.hpp"
#include "allfetch/system/linux/memory_info.hpp"
#include "allfetch/system/linux/cpu_info.hpp"
#include "allfetch/system/linux/gpu_info.hpp"
#include "allfetch/system/shell_info.hpp"
#include "allfetch/system/terminal_info.hpp"

namespace allfetch::system
{

    /* Implementation of the getSystemInfo function
    Output:
        SystemInfo: The system information
    */
    SystemInfo getSystemInfo()
    {
        SystemInfo info;

        info.osInfo = getLinuxInfo();
        info.memoryInfo = getMemoryInfo();
        info.cpuInfo = getCPUInfo();
        info.gpuInfo = getGPUInfo();
        info.terminalInfo = getTerminalInfo();
        info.shellInfo = getShellInfo();

        return info;
    }

}
