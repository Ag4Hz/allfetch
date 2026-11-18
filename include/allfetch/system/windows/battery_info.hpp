#pragma once

#include "allfetch/system/system_info.hpp"

#include <windows.h>
#include <setupapi.h>
#include <winioctl.h>
#include <poclass.h>

#include <algorithm>
#include <cstdio>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace allfetch::system
{
    namespace battery_detail
    {
        // Local definition avoids an external GUID symbol dependency.
        inline constexpr GUID batteryInterfaceGuid{
            0x72631e54, 0x78a4, 0x11d0, {0xbc, 0xf7, 0x00, 0xaa, 0x00, 0xb7, 0xb3, 0x2a}};

        struct DeviceList
        {
            HDEVINFO value;

            ~DeviceList()
            {
                if (value != INVALID_HANDLE_VALUE)
                    SetupDiDestroyDeviceInfoList(value);
            }
        };

        struct HandleCloser
        {
            void operator()(void *handle) const
            {
                CloseHandle(handle);
            }
        };

        template <typename T>
        inline bool query(
            HANDLE handle,
            ULONG tag,
            BATTERY_QUERY_INFORMATION_LEVEL level,
            T &value)
        {
            BATTERY_QUERY_INFORMATION request{};
            request.BatteryTag = tag;
            request.InformationLevel = level;

            DWORD returned = 0;

            return DeviceIoControl(
                       handle,
                       IOCTL_BATTERY_QUERY_INFORMATION,
                       &request,
                       sizeof(request),
                       &value,
                       sizeof(value),
                       &returned,
                       nullptr) &&
                   returned >= sizeof(value);
        }

        inline std::string queryString(
            HANDLE handle,
            ULONG tag,
            BATTERY_QUERY_INFORMATION_LEVEL level)
        {
            BATTERY_QUERY_INFORMATION request{};
            request.BatteryTag = tag;
            request.InformationLevel = level;

            for (DWORD count = 128; count <= 4096; count *= 2)
            {
                std::vector<wchar_t> buffer(count);
                DWORD returned = 0;

                if (!DeviceIoControl(
                        handle,
                        IOCTL_BATTERY_QUERY_INFORMATION,
                        &request,
                        sizeof(request),
                        buffer.data(),
                        static_cast<DWORD>(
                            buffer.size() * sizeof(wchar_t)),
                        &returned,
                        nullptr))
                {
                    const DWORD error = GetLastError();

                    if (error == ERROR_INSUFFICIENT_BUFFER || error == ERROR_MORE_DATA)
                        continue;

                    return "Unknown";
                }

                if (returned > buffer.size() * sizeof(wchar_t) || returned % sizeof(wchar_t) != 0)
                    return "Unknown";

                const auto end = std::find(
                    buffer.begin(),
                    buffer.begin() + returned / sizeof(wchar_t),
                    L'\0');

                const int length =
                    static_cast<int>(end - buffer.begin());

                if (length == 0)
                    return "Unknown";

                const int bytes = WideCharToMultiByte(
                    CP_UTF8, WC_ERR_INVALID_CHARS,
                    buffer.data(), length,
                    nullptr, 0, nullptr, nullptr);

                if (bytes <= 0)
                    return "Unknown";

                std::string result(bytes, '\0');

                if (!WideCharToMultiByte(
                        CP_UTF8, WC_ERR_INVALID_CHARS,
                        buffer.data(), length,
                        result.data(), bytes, nullptr, nullptr))
                    return "Unknown";

                return result;
            }

            return "Unknown";
        }

        inline BatteryInfo collect(
            HANDLE handle,
            ULONG tag,
            const BATTERY_INFORMATION &hardware)
        {
            BatteryInfo info;

            info.manufacturer = queryString(
                handle, tag, BatteryManufactureName);

            info.modelName = queryString(
                handle, tag, BatteryDeviceName);

            info.serial = queryString(
                handle, tag, BatterySerialNumber);

            if (hardware.Technology == 1)
                info.technology = "Rechargeable";
            else if (hardware.Technology == 0)
                info.technology = "Non-rechargeable";

            // Windows uses zero when the cycle count is unsupported.
            if (hardware.CycleCount > 0 && hardware.CycleCount <=
                                               static_cast<ULONG>(
                                                   (std::numeric_limits<int>::max)()))
            {
                info.cycleCount =
                    static_cast<int>(hardware.CycleCount);
            }

            BATTERY_MANUFACTURE_DATE date{};

            if (query(handle, tag, BatteryManufactureDate, date) && date.Year >= 1900 && date.Year <= 9999 && date.Month >= 1 && date.Month <= 12 && date.Day >= 1 && date.Day <= 31)
            {
                char text[16]{};

                std::snprintf(
                    text, sizeof(text), "%04u-%02u-%02u",
                    static_cast<unsigned>(date.Year),
                    static_cast<unsigned>(date.Month),
                    static_cast<unsigned>(date.Day));

                info.manufactureDate = text;
            }

            ULONG temperature = 0;

            if (query(handle, tag, BatteryTemperature, temperature) && temperature != 0 && temperature != 0xffffffffUL)
            {
                // Windows reports tenths of a kelvin.
                info.temperatureCelsius =
                    temperature / 10.0 - 273.15;
            }

            BATTERY_WAIT_STATUS request{};
            request.BatteryTag = tag;
            request.Timeout = 0;

            BATTERY_STATUS state{};
            DWORD returned = 0;

            if (!DeviceIoControl(
                    handle,
                    IOCTL_BATTERY_QUERY_STATUS,
                    &request,
                    sizeof(request),
                    &state,
                    sizeof(state),
                    &returned,
                    nullptr) ||
                returned < sizeof(state))
            {
                return info;
            }

            if (state.Capacity != BATTERY_UNKNOWN_CAPACITY && hardware.FullChargedCapacity != BATTERY_UNKNOWN_CAPACITY && hardware.FullChargedCapacity > 0)
            {
                info.capacityPercent =
                    100.0 * state.Capacity / hardware.FullChargedCapacity;
            }

            if (state.PowerState & BATTERY_POWER_ON_LINE)
                info.status.emplace_back("AC Connected");

            if (state.PowerState & BATTERY_CHARGING)
                info.status.emplace_back("Charging");

            if (state.PowerState & BATTERY_DISCHARGING)
                info.status.emplace_back("Discharging");

            if (state.PowerState & BATTERY_CRITICAL)
                info.status.emplace_back("Critical");

            if (state.PowerState & BATTERY_DISCHARGING)
            {
                ULONG seconds = BATTERY_UNKNOWN_TIME;

                if (query(handle, tag, BatteryEstimatedTime, seconds) && seconds != BATTERY_UNKNOWN_TIME)
                {
                    info.timeRemainingSeconds = seconds;
                }
            }

            return info;
        }
    }

    inline std::vector<BatteryInfo> getBatteryInfo()
    {
        std::vector<BatteryInfo> result;

        battery_detail::DeviceList devices{
            SetupDiGetClassDevsW(
                &battery_detail::batteryInterfaceGuid,
                nullptr, nullptr,
                DIGCF_PRESENT | DIGCF_DEVICEINTERFACE)};

        if (devices.value == INVALID_HANDLE_VALUE)
            return result;

        for (DWORD index = 0;; ++index)
        {
            SP_DEVICE_INTERFACE_DATA interfaceData{};
            interfaceData.cbSize = sizeof(interfaceData);

            if (!SetupDiEnumDeviceInterfaces(
                    devices.value, nullptr,
                    &battery_detail::batteryInterfaceGuid,
                    index, &interfaceData))
                break;

            DWORD required = 0;

            SetupDiGetDeviceInterfaceDetailW(
                devices.value, &interfaceData,
                nullptr, 0, &required, nullptr);

            if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || required <
                                                                   sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W))
                continue;

            // LocalAlloc supplies the required alignment.
            std::unique_ptr<void, decltype(&LocalFree)> storage(
                LocalAlloc(LPTR, required), &LocalFree);

            if (!storage)
                continue;

            auto *detail =
                static_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W *>(
                    storage.get());

            detail->cbSize =
                sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

            if (!SetupDiGetDeviceInterfaceDetailW(
                    devices.value, &interfaceData,
                    detail, required, nullptr, nullptr))
                continue;

            HANDLE rawHandle = CreateFileW(
                detail->DevicePath,
                GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr, OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL, nullptr);

            if (rawHandle == INVALID_HANDLE_VALUE)
                continue;

            std::unique_ptr<void, battery_detail::HandleCloser>
                handle(rawHandle);

            ULONG timeout = 0;
            ULONG tag = 0;
            DWORD returned = 0;

            if (!DeviceIoControl(
                    handle.get(),
                    IOCTL_BATTERY_QUERY_TAG,
                    &timeout, sizeof(timeout),
                    &tag, sizeof(tag),
                    &returned, nullptr) ||
                returned < sizeof(tag) || tag == BATTERY_TAG_INVALID)
                continue;

            BATTERY_INFORMATION hardware{};

            if (!battery_detail::query(
                    handle.get(), tag,
                    BatteryInformation, hardware))
                continue;

            // Ignore batteries that cannot power the computer.
            if (!(hardware.Capabilities & BATTERY_SYSTEM_BATTERY))
                continue;

            result.push_back(
                battery_detail::collect(
                    handle.get(), tag, hardware));
        }

        return result;
    }
}