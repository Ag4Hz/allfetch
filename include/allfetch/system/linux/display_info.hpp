#pragma once

#include "allfetch/system/system_info.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#if defined(ALLFETCH_HAS_XRANDR)
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#endif

namespace allfetch::system
{
    namespace linux_display_detail
    {
#if defined(ALLFETCH_HAS_XRANDR)
        inline double refreshRate(const XRRModeInfo &mode)
        {
            if (mode.dotClock == 0 ||
                mode.hTotal == 0 || mode.vTotal == 0)
            {
                return -1.0;
            }

            double rate = static_cast<double>(mode.dotClock) /
                          (static_cast<double>(mode.hTotal) *
                           mode.vTotal);

            if (mode.modeFlags & RR_Interlace)
            {
                rate *= 2.0;
            }

            if (mode.modeFlags & RR_DoubleScan)
            {
                rate /= 2.0;
            }

            return rate;
        }

        inline std::vector<DisplayInfo> collectXrandr()
        {
            std::vector<DisplayInfo> result;

            std::unique_ptr<Display, decltype(&XCloseDisplay)>
                display(XOpenDisplay(nullptr), &XCloseDisplay);

            if (!display)
            {
                return result;
            }

            int major = 1;
            int minor = 2;

            if (!XRRQueryVersion(display.get(), &major, &minor) ||
                major < 1 || (major == 1 && minor < 2))
            {
                return result;
            }

            const bool version13 =
                major > 1 || (major == 1 && minor >= 3);

            for (int screen = 0;
                 screen < ScreenCount(display.get());
                 ++screen)
            {
                const Window root = RootWindow(display.get(), screen);

                std::unique_ptr<
                    XRRScreenResources,
                    decltype(&XRRFreeScreenResources)>
                    resources(
                        XRRGetScreenResources(display.get(), root),
                        &XRRFreeScreenResources);

                if (!resources)
                {
                    continue;
                }

                const RROutput primary = version13
                                             ? XRRGetOutputPrimary(display.get(), root)
                                             : None;

                for (int index = 0;
                     index < resources->noutput;
                     ++index)
                {
                    const RROutput id = resources->outputs[index];

                    std::unique_ptr<
                        XRROutputInfo,
                        decltype(&XRRFreeOutputInfo)>
                        output(
                            XRRGetOutputInfo(
                                display.get(), resources.get(), id),
                            &XRRFreeOutputInfo);

                    if (!output || output->connection != RR_Connected ||
                        output->crtc == None)
                    {
                        continue;
                    }

                    std::unique_ptr<
                        XRRCrtcInfo,
                        decltype(&XRRFreeCrtcInfo)>
                        crtc(
                            XRRGetCrtcInfo(
                                display.get(), resources.get(), output->crtc),
                            &XRRFreeCrtcInfo);

                    if (!crtc || crtc->mode == None)
                    {
                        continue;
                    }

                    DisplayInfo info;
                    info.id = static_cast<long long>(id);
                    info.name.assign(output->name, output->nameLen);
                    info.platformApi = "XRandR";
                    info.primaryKnown = version13 && primary != None;
                    info.primary = info.primaryKnown && id == primary;
                    info.width = static_cast<int>(crtc->width);
                    info.height = static_cast<int>(crtc->height);

                    if (output->mm_width > 0)
                    {
                        info.physicalWidthMm =
                            static_cast<int>(output->mm_width);
                    }

                    if (output->mm_height > 0)
                    {
                        info.physicalHeightMm =
                            static_cast<int>(output->mm_height);
                    }

                    if (crtc->rotation & RR_Rotate_0)
                        info.rotationDegrees = 0;
                    else if (crtc->rotation & RR_Rotate_90)
                        info.rotationDegrees = 90;
                    else if (crtc->rotation & RR_Rotate_180)
                        info.rotationDegrees = 180;
                    else if (crtc->rotation & RR_Rotate_270)
                        info.rotationDegrees = 270;

                    for (int modeIndex = 0;
                         modeIndex < resources->nmode;
                         ++modeIndex)
                    {
                        const auto &mode = resources->modes[modeIndex];

                        if (mode.id == crtc->mode)
                        {
                            info.refreshRateHz = refreshRate(mode);
                        }

                        if (output->npreferred > 0 &&
                            mode.id == output->modes[0])
                        {
                            info.preferredWidth =
                                static_cast<int>(mode.width);
                            info.preferredHeight =
                                static_cast<int>(mode.height);
                            info.preferredRefreshRateHz =
                                refreshRate(mode);
                        }
                    }

                    result.push_back(info);
                }
            }

            return result;
        }
#endif

        inline std::vector<DisplayInfo> collectSysfs()
        {
            namespace fs = std::filesystem;
            std::vector<DisplayInfo> result;
            std::error_code error;

            fs::directory_iterator entry("/sys/class/drm", error);
            const fs::directory_iterator end;

            while (!error && entry != end)
            {
                std::ifstream statusFile(entry->path() / "status");
                std::string status;
                std::getline(statusFile, status);

                if (status == "connected")
                {
                    DisplayInfo info;
                    info.name = entry->path().filename().string();
                    info.platformApi = "DRM sysfs";
                    result.push_back(info);
                }

                entry.increment(error);
            }

            return result;
        }
    }

    inline std::vector<DisplayInfo> getDisplayInfo()
    {
#if defined(ALLFETCH_HAS_XRANDR)
        auto displays = linux_display_detail::collectXrandr();

        if (!displays.empty())
        {
            return displays;
        }
#endif

        return linux_display_detail::collectSysfs();
    }
}