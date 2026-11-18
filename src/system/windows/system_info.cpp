#include "allfetch/system/windows/os_info.hpp"
#include "allfetch/system/system_info.hpp"
#include "allfetch/system/windows/memory_info.hpp"
#include "allfetch/system/windows/cpu_info.hpp"
#include "allfetch/system/windows/gpu_info.hpp"
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

        info.osInfo = getWindowsInfo();
        info.memoryInfo = getMemoryInfo();
        info.cpuInfo = getCPUInfo();
        info.gpuInfo = getGPUInfo();
        info.terminalInfo = getTerminalInfo();
        info.shellInfo = getShellInfo();

        return info;
    }
}
