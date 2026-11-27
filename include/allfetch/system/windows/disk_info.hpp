#pragma once

#include "allfetch/system/system_info.hpp"

#include "allfetch/system/windows/platform.hpp"
#include <winioctl.h>
#include <winternl.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <cwchar>
#include <limits>
#include <memory>
#include <set>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace allfetch::system
{
    namespace disk_detail
    {
        struct HandleCloser
        {
            void operator()(void *handle) const
            {
                CloseHandle(handle);
            }
        };

        struct SearchCloser
        {
            void operator()(void *handle) const
            {
                FindVolumeClose(handle);
            }
        };

        using DeviceHandle = std::unique_ptr<void, HandleCloser>;

        inline DeviceHandle openDevice(const std::wstring &path)
        {
            HANDLE handle = CreateFileW(
                path.c_str(),
                0,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr,
                OPEN_EXISTING,
                0,
                nullptr);

            return DeviceHandle(
                handle == INVALID_HANDLE_VALUE ? nullptr : handle);
        }

        using windows_platform::utf8;

        inline long long signedBytes(ULONGLONG value)
        {
            if (value > static_cast<ULONGLONG>(
                            (std::numeric_limits<long long>::max)()))
                return -1;

            return static_cast<long long>(value);
        }

        inline std::vector<std::wstring> mountPaths(
            const std::wstring &volume)
        {
            std::vector<wchar_t> buffer(256);
            DWORD required = 0;

            while (!GetVolumePathNamesForVolumeNameW(
                volume.c_str(),
                buffer.data(),
                static_cast<DWORD>(buffer.size()),
                &required))
            {
                if (GetLastError() != ERROR_MORE_DATA || required <= buffer.size())
                    return {};

                buffer.resize(required);
            }

            std::vector<std::wstring> result;

            for (const wchar_t *path = buffer.data();
                 *path != L'\0';
                 path += std::wcslen(path) + 1)
            {
                result.emplace_back(path);
            }

            return result;
        }

        inline std::string devicePath(
            const std::wstring &volume)
        {
            // "\\?\Volume{...}\" becomes "Volume{...}".
            const std::wstring name =
                volume.substr(4, volume.size() - 5);

            std::vector<wchar_t> buffer(1024);

            while (!QueryDosDeviceW(
                name.c_str(),
                buffer.data(),
                static_cast<DWORD>(buffer.size())))
            {
                if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || buffer.size() >= 65536)
                    return "Unknown";

                buffer.resize(buffer.size() * 2);
            }

            return utf8(buffer.data());
        }

        inline std::string physicalType(DWORD diskNumber)
        {
            const auto handle = openDevice(
                L"\\\\.\\PhysicalDrive" + std::to_wstring(diskNumber));

            if (!handle)
                return "Unknown";

            STORAGE_PROPERTY_QUERY request{};
            request.PropertyId = StorageDeviceProperty;
            request.QueryType = PropertyStandardQuery;

            alignas(STORAGE_DEVICE_DESCRIPTOR) unsigned char buffer[4096]{};

            DWORD returned = 0;

            if (!DeviceIoControl(
                    handle.get(),
                    IOCTL_STORAGE_QUERY_PROPERTY,
                    &request, sizeof(request),
                    buffer, sizeof(buffer),
                    &returned, nullptr) ||
                returned < sizeof(STORAGE_DEVICE_DESCRIPTOR))
                return "Unknown";

            STORAGE_DEVICE_DESCRIPTOR device{};
            std::memcpy(&device, buffer, sizeof(device));

            if (device.BusType == BusTypeSd)
                return "SD";

            if (device.BusType == BusTypeMmc)
                return "MMC";

            // SCSI peripheral type zero means a direct-access device.
            if (device.DeviceType != 0)
                return "Unknown";

            if (device.BusType == BusTypeNvme)
                return "SSD";

            // Avoid treating USB flash drives and virtual disks as SSDs.
            if (device.BusType != BusTypeAta && device.BusType != BusTypeSata && device.BusType != BusTypeSas)
                return "Unknown";

            request.PropertyId = StorageDeviceSeekPenaltyProperty;

            DEVICE_SEEK_PENALTY_DESCRIPTOR seek{};

            if (!DeviceIoControl(
                    handle.get(),
                    IOCTL_STORAGE_QUERY_PROPERTY,
                    &request, sizeof(request),
                    &seek, sizeof(seek),
                    &returned, nullptr) ||
                returned < sizeof(seek))
                return "Unknown";

            return seek.IncursSeekPenalty ? "HDD" : "SSD";
        }

        inline std::string volumeType(HANDLE handle, std::map<DWORD, std::string> &physicalTypes)
        {
            // A fixed limit keeps unusual or unsupported layouts safe.
            alignas(VOLUME_DISK_EXTENTS) unsigned char buffer[65536]{};

            DWORD returned = 0;

            if (!DeviceIoControl(
                    handle,
                    IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                    nullptr, 0,
                    buffer, sizeof(buffer),
                    &returned, nullptr))
                return "Unknown";

            constexpr std::size_t offset =
                offsetof(VOLUME_DISK_EXTENTS, Extents);

            if (returned < offset)
                return "Unknown";

            DWORD count = 0;
            std::memcpy(&count, buffer, sizeof(count));

            if (count == 0 || count > (returned - offset) / sizeof(DISK_EXTENT))
                return "Unknown";

            std::set<std::string> types;
            std::set<DWORD> disks;

            for (DWORD index = 0; index < count; ++index)
            {
                DISK_EXTENT extent{};

                std::memcpy(
                    &extent,
                    buffer + offset + index * sizeof(extent),
                    sizeof(extent));

                disks.insert(extent.DiskNumber);
            }

            for (DWORD disk : disks)
            {
                auto [entry, inserted] = physicalTypes.try_emplace(disk);
                if (inserted)
                    entry->second = physicalType(disk);
                types.insert(entry->second);
            }

            if (types.contains("Unknown"))
                return "Unknown";

            return types.size() == 1 ? *types.begin() : "Mixed";
        }

        inline long long creationTime(HANDLE handle)
        {
            using QueryVolume = NTSTATUS(NTAPI *)(
                HANDLE,
                PIO_STATUS_BLOCK,
                PVOID,
                ULONG,
                FS_INFORMATION_CLASS);

            static const auto query =
                reinterpret_cast<QueryVolume>(
                    GetProcAddress(
                        GetModuleHandleW(L"ntdll.dll"),
                        "NtQueryVolumeInformationFile"));

            if (!query)
                return -1;

            alignas(FILE_FS_VOLUME_INFORMATION) unsigned char buffer[4096]{};

            IO_STATUS_BLOCK status{};

            if (query(
                    handle,
                    &status,
                    buffer,
                    sizeof(buffer),
                    FileFsVolumeInformation) < 0 ||
                status.Information <
                    offsetof(FILE_FS_VOLUME_INFORMATION, VolumeLabel))
                return -1;

            LARGE_INTEGER created{};
            std::memcpy(&created, buffer, sizeof(created));

            constexpr long long unixEpoch =
                116444736000000000LL;

            if (created.QuadPart < unixEpoch)
                return -1;

            // Windows ticks are 100 ns; the result is Unix milliseconds.
            return (created.QuadPart - unixEpoch) / 10000;
        }
    }

    inline std::vector<DiskInfo> getDiskInfo()
    {
        std::vector<DiskInfo> result;
        std::map<DWORD, std::string> physicalTypes;
        wchar_t volumeBuffer[256]{};

        HANDLE rawSearch = FindFirstVolumeW(
            volumeBuffer,
            static_cast<DWORD>(
                sizeof(volumeBuffer) / sizeof(wchar_t)));

        if (rawSearch == INVALID_HANDLE_VALUE)
            return result;

        std::unique_ptr<void, disk_detail::SearchCloser>
            search(rawSearch);

        do
        {
            const std::wstring volume(volumeBuffer);
            const auto paths = disk_detail::mountPaths(volume);

            // Skip volumes without an accessible mount path.
            if (paths.empty())
                continue;

            DiskInfo base;
            base.mountFrom = disk_detail::devicePath(volume);

            wchar_t label[256]{};
            wchar_t filesystem[256]{};
            DWORD flags = 0;

            if (GetVolumeInformationW(
                    volume.c_str(),
                    label, 256,
                    nullptr, nullptr,
                    &flags,
                    filesystem, 256))
            {
                base.name = disk_detail::utf8(label);
                base.filesystem = disk_detail::utf8(filesystem);
            }

            ULARGE_INTEGER total{};
            ULARGE_INTEGER free{};

            if (GetDiskFreeSpaceExW(
                    volume.c_str(),
                    nullptr,
                    &total,
                    &free))
            {
                base.totalBytes =
                    disk_detail::signedBytes(total.QuadPart);

                // Reject incompatible totals, for example under quotas.
                if (free.QuadPart <= total.QuadPart)
                    base.freeBytes =
                        disk_detail::signedBytes(free.QuadPart);

                if (base.totalBytes >= 0 && base.freeBytes >= 0)
                    base.usedBytes =
                        base.totalBytes - base.freeBytes;
            }

            // A volume device path must not end with a backslash.
            const auto handle = disk_detail::openDevice(
                volume.substr(0, volume.size() - 1));

            if (handle)
            {
                base.type =
                    disk_detail::volumeType(handle.get(), physicalTypes);

                base.creationTimeMs =
                    disk_detail::creationTime(handle.get());
            }

            for (const auto &path : paths)
            {
                DiskInfo info = base;
                info.mountpoint = disk_detail::utf8(path);

                const UINT driveType = GetDriveTypeW(path.c_str());

                info.volumeType.emplace_back(
                    driveType == DRIVE_REMOVABLE
                        ? "Removable"
                        : "Regular");

                if (flags & FILE_READ_ONLY_VOLUME)
                    info.volumeType.emplace_back("ReadOnly");

                result.push_back(std::move(info));
            }

        } while (FindNextVolumeW(
            search.get(),
            volumeBuffer,
            static_cast<DWORD>(
                sizeof(volumeBuffer) / sizeof(wchar_t))));

        std::sort(
            result.begin(), result.end(),
            [](const DiskInfo &left, const DiskInfo &right)
            {
                return left.mountpoint < right.mountpoint;
            });

        return result;
    }
}