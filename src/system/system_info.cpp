#if defined(_WIN32)
#include <winsock2.h>
#include "allfetch/system/windows/os_info.hpp"
#include "allfetch/system/windows/memory_info.hpp"
#include "allfetch/system/windows/gpu_info.hpp"
#include "allfetch/system/windows/cpu_info.hpp"
#include "allfetch/system/windows/display_info.hpp"
#include "allfetch/system/windows/battery_info.hpp"
#include "allfetch/system/windows/disk_info.hpp"
#include "allfetch/system/windows/title_info.hpp"
#include "allfetch/system/windows/window_manager_info.hpp"
#include "allfetch/system/windows/desktop_appearance_info.hpp"
#elif defined(__linux__)
#include "allfetch/system/linux/os_info.hpp"
#include "allfetch/system/linux/packages_info.hpp"
#include "allfetch/system/linux/session_info.hpp"
#include "allfetch/system/linux/memory_info.hpp"
#include "allfetch/system/linux/gpu_info.hpp"
#include "allfetch/system/linux/cpu_info.hpp"
#include "allfetch/system/linux/display_info.hpp"
#include "allfetch/system/linux/battery_info.hpp"
#include "allfetch/system/linux/disk_info.hpp"
#include "allfetch/system/linux/title_info.hpp"
#include "allfetch/system/linux/window_manager_info.hpp"
#include "allfetch/system/linux/desktop_appearance_info.hpp"
#endif

#include "allfetch/system/system_info.hpp"
#include "allfetch/system/shell_info.hpp"
#include "allfetch/system/terminal_info.hpp"
#include "allfetch/system/uptime_info.hpp"
#include "allfetch/system/swap_info.hpp"
#include "allfetch/system/local_ip_info.hpp"
#include "allfetch/system/host_info.hpp"
#include "allfetch/system/locale_info.hpp"
#include "allfetch/system/colors_info.hpp"

namespace allfetch::system
{

    /* Implementation of the getSystemInfo function
    Output:
        SystemInfo: The system information
    */
    SystemInfo getSystemInfo(bool collectColors, bool collectFull, [[maybe_unused]] bool collectHID)
    {
        SystemInfo info;

        info.osInfo = getOSInfo();
        info.memoryInfo = getMemoryInfo();
        info.cpuInfo = getCPUInfo();
        info.gpuInfo = getGPUInfo();
        info.terminalInfo = getTerminalInfo(nlohmann::json::object(), {}, collectFull);
        info.shellInfo = getShellInfo();
        info.swapInfo = getSwapInfo();
        info.localIPInfo = getLocalIPInfo();
        info.hostInfo = getHostInfo();
        info.displayInfo = getDisplayInfo();
        info.diskInfo = getDiskInfo();
        info.batteryInfo = getBatteryInfo();
        info.titleInfo = getTitleInfo();

        if (collectColors)
        {
            info.colorsInfo = getColorsInfo();
        }

        if (collectFull)
        {
            info.uptimeInfo = getUptimeInfo();
            info.localeInfo = getLocaleInfo();
            info.windowManagerInfo = getWindowManagerInfo();
            info.desktopAppearanceInfo = getDesktopAppearanceInfo();
        }

#if defined(__linux__)
        info.packagesInfo = getPackagesInfo();
#endif
#if defined(__linux__)
        linux_session::collect(info, collectColors, collectFull);
#endif
        return info;
    }
}
