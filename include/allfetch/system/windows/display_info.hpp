#pragma once

#include "allfetch/system/system_info.hpp"

#include <windows.h>
#include <shellscalingapi.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace allfetch::system
{
    namespace windows_display_detail
    {
        inline std::string utf8(const wchar_t *value)
        {
            const int size = WideCharToMultiByte(
                CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);

            if (size <= 1)
            {
                return "Unknown";
            }

            std::string result(size, '\0');

            if (!WideCharToMultiByte(
                    CP_UTF8, 0, value, -1,
                    result.data(), size, nullptr, nullptr))
            {
                return "Unknown";
            }

            result.pop_back();
            return result;
        }
    }

    inline std::vector<DisplayInfo> getDisplayInfo()
    {
        std::vector<DisplayInfo> result;
        constexpr UINT32 flags = QDC_ONLY_ACTIVE_PATHS;

        std::vector<DISPLAYCONFIG_PATH_INFO> paths;
        std::vector<DISPLAYCONFIG_MODE_INFO> modes;

        bool collected = false;

        for (int attempt = 0; attempt < 3; ++attempt)
        {
            UINT32 pathCount = 0;
            UINT32 modeCount = 0;

            if (GetDisplayConfigBufferSizes(
                    flags, &pathCount, &modeCount) != ERROR_SUCCESS ||
                pathCount == 0 || modeCount == 0)
            {
                return result;
            }

            paths.resize(pathCount);
            modes.resize(modeCount);

            const LONG status = QueryDisplayConfig(
                flags, &pathCount, paths.data(),
                &modeCount, modes.data(), nullptr);

            if (status == ERROR_INSUFFICIENT_BUFFER)
            {
                continue;
            }

            if (status != ERROR_SUCCESS)
            {
                return result;
            }

            paths.resize(pathCount);
            modes.resize(modeCount);
            collected = true;
            break;
        }

        if (!collected)
        {
            return result;
        }

        for (const auto &path : paths)
        {
            DisplayInfo info;
            info.platformApi = "QueryDisplayConfig";

            const auto sourceIndex = path.sourceInfo.modeInfoIdx;

            if (sourceIndex >= modes.size() ||
                modes[sourceIndex].infoType !=
                    DISPLAYCONFIG_MODE_INFO_TYPE_SOURCE)
            {
                continue;
            }

            const auto &source = modes[sourceIndex].sourceMode;
            info.width = static_cast<int>(source.width);
            info.height = static_cast<int>(source.height);

            const auto frequency = path.targetInfo.refreshRate;

            if (frequency.Numerator != 0 &&
                frequency.Denominator != 0)
            {
                info.refreshRateHz =
                    static_cast<double>(frequency.Numerator) /
                    frequency.Denominator;
            }

            DISPLAYCONFIG_TARGET_DEVICE_NAME name{};
            name.header.type =
                DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
            name.header.size = sizeof(name);
            name.header.adapterId = path.targetInfo.adapterId;
            name.header.id = path.targetInfo.id;

            if (DisplayConfigGetDeviceInfo(
                    &name.header) == ERROR_SUCCESS)
            {
                info.name = windows_display_detail::utf8(
                    name.monitorFriendlyDeviceName);

                if (info.name == "Unknown")
                {
                    info.name = windows_display_detail::utf8(
                        name.monitorDevicePath);
                }
            }

            DISPLAYCONFIG_TARGET_PREFERRED_MODE preferred{};
            preferred.header.type =
                DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_PREFERRED_MODE;
            preferred.header.size = sizeof(preferred);
            preferred.header.adapterId = path.targetInfo.adapterId;
            preferred.header.id = path.targetInfo.id;

            if (DisplayConfigGetDeviceInfo(
                    &preferred.header) == ERROR_SUCCESS)
            {
                info.preferredWidth =
                    static_cast<int>(preferred.width);
                info.preferredHeight =
                    static_cast<int>(preferred.height);

                const auto rate =
                    preferred.targetMode.targetVideoSignalInfo.vSyncFreq;

                if (rate.Numerator != 0 && rate.Denominator != 0)
                {
                    info.preferredRefreshRateHz =
                        static_cast<double>(rate.Numerator) /
                        rate.Denominator;
                }
            }

            switch (path.targetInfo.rotation)
            {
            case DISPLAYCONFIG_ROTATION_IDENTITY:
                info.rotationDegrees = 0;
                break;
            case DISPLAYCONFIG_ROTATION_ROTATE90:
                info.rotationDegrees = 90;
                break;
            case DISPLAYCONFIG_ROTATION_ROTATE180:
                info.rotationDegrees = 180;
                break;
            case DISPLAYCONFIG_ROTATION_ROTATE270:
                info.rotationDegrees = 270;
                break;
            }

            switch (path.targetInfo.outputTechnology)
            {
            case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_INTERNAL:
            case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_DISPLAYPORT_EMBEDDED:
            case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_UDI_EMBEDDED:
                info.type = "Builtin";
                break;
            case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_HDMI:
            case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_DVI:
            case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_HD15:
            case DISPLAYCONFIG_OUTPUT_TECHNOLOGY_DISPLAYPORT_EXTERNAL:
                info.type = "External";
                break;
            }

            const POINT position{
                source.position.x, source.position.y};

            const HMONITOR monitor = MonitorFromPoint(
                position, MONITOR_DEFAULTTONULL);

            if (monitor != nullptr)
            {
                info.id = static_cast<long long>(
                    reinterpret_cast<std::uintptr_t>(monitor));

                MONITORINFO details{};
                details.cbSize = sizeof(details);

                if (GetMonitorInfoW(monitor, &details))
                {
                    info.primaryKnown = true;
                    info.primary =
                        (details.dwFlags & MONITORINFOF_PRIMARY) != 0;
                }

                DEVICE_SCALE_FACTOR scale{};

                if (SUCCEEDED(GetScaleFactorForMonitor(
                        monitor, &scale)))
                {
                    const int percent = static_cast<int>(scale);

                    if (percent > 0)
                    {
                        info.effectiveDpi = static_cast<int>(
                            std::lround(96.0 * percent / 100.0));

                        info.scaledWidth = static_cast<int>(
                            std::lround(info.width * 100.0 / percent));

                        info.scaledHeight = static_cast<int>(
                            std::lround(info.height * 100.0 / percent));
                    }
                }
            }

            DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO_2 color{};

            color.header.type =
                DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO_2;
            color.header.size = sizeof(color);
            color.header.adapterId = path.targetInfo.adapterId;
            color.header.id = path.targetInfo.id;

            const LONG colorStatus = DisplayConfigGetDeviceInfo(&color.header);

            if (colorStatus == ERROR_SUCCESS)
            {
                info.bitsPerColorChannel =
                    static_cast<int>(color.bitsPerColorChannel);

                if (color.highDynamicRangeUserEnabled)
                {
                    info.hdrStatus = "Enabled";
                }
                else if (color.highDynamicRangeSupported)
                {
                    info.hdrStatus = "Supported";
                }
                else
                {
                    info.hdrStatus = "Unsupported";
                }
            }

            result.push_back(info);
        }

        return result;
    }
}