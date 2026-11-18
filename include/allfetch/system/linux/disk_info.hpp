#pragma once

#include "allfetch/system/system_info.hpp"

#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace allfetch::system
{
    namespace disk_detail
    {
        namespace fs = std::filesystem;

        inline std::string text(const fs::path &path)
        {
            std::ifstream file(path);
            std::string value;
            std::getline(file, value);
            return value;
        }

        inline std::string unescapeMount(const std::string &value)
        {
            std::string result;

            for (std::size_t index = 0; index < value.size(); ++index)
            {
                if (value[index] == '\\' && index + 3 < value.size() && value[index + 1] >= '0' && value[index + 1] <= '7' && value[index + 2] >= '0' && value[index + 2] <= '7' && value[index + 3] >= '0' && value[index + 3] <= '7')
                {
                    const int byte =
                        (value[index + 1] - '0') * 64 + (value[index + 2] - '0') * 8 + (value[index + 3] - '0');

                    result.push_back(static_cast<char>(byte));
                    index += 3;
                }
                else
                {
                    result.push_back(value[index]);
                }
            }

            return result;
        }

        inline std::string deviceNumber(dev_t device)
        {
            return std::to_string(major(device)) + ":" + std::to_string(minor(device));
        }

        inline long long signedCount(std::uint64_t value)
        {
            if (value > static_cast<std::uint64_t>(
                            std::numeric_limits<long long>::max()))
                return -1;

            return static_cast<long long>(value);
        }

        inline long long bytes(
            std::uint64_t blocks,
            std::uint64_t blockSize)
        {
            const auto limit = static_cast<std::uint64_t>(
                std::numeric_limits<long long>::max());

            if (blockSize == 0 || blocks > limit / blockSize)
                return -1;

            return static_cast<long long>(blocks * blockSize);
        }

        inline std::string medium(
            fs::path node,
            unsigned depth = 0)
        {
            if (depth > 16)
                return "Unknown";

            std::error_code error;
            node = fs::canonical(node, error);

            if (error)
                return "Unknown";

            // A partition inherits its parent disk's medium.
            if (fs::exists(node / "partition", error))
                node = node.parent_path();

            if (error)
                return "Unknown";

            const std::string cardType =
                text(node / "device/type");

            if (cardType == "SD" || cardType == "SDcombo")
                return "SD";

            if (cardType == "MMC")
                return "MMC";

            // Resolve device-mapper and software RAID backing devices.
            std::set<std::string> types;

            fs::directory_iterator child(node / "slaves", error);
            const fs::directory_iterator end;

            if (error)
                return "Unknown";

            while (!error && child != end)
            {
                types.insert(medium(child->path(), depth + 1));
                child.increment(error);
            }

            if (error)
                return "Unknown";

            if (!types.empty())
            {
                if (types.contains("Unknown"))
                    return "Unknown";

                return types.size() == 1
                           ? *types.begin()
                           : "Mixed";
            }

            std::string location = node.string();

            std::transform(
                location.begin(), location.end(), location.begin(),
                [](unsigned char character)
                {
                    return static_cast<char>(
                        std::tolower(character));
                });

            // These paths do not establish the host's physical medium.
            if (location.find("/virtual/") != std::string::npos || location.find("/virtio") != std::string::npos || location.find("vmbus") != std::string::npos || location.find("/xen") != std::string::npos)
                return "Unknown";

            // A USB bridge may hide an SD card or another flash device.
            if (location.find("/usb") != std::string::npos)
                return "Unknown";

            const std::string rotational =
                text(node / "queue/rotational");

            if (rotational == "1")
                return "HDD";

            if (rotational == "0")
            {
                // Restrict the SSD inference to common internal devices.
                if (node.filename().string().starts_with("nvme") || location.find("/ata") != std::string::npos)
                    return "SSD";
            }

            return "Unknown";
        }

        inline int hexDigit(char character)
        {
            if (character >= '0' && character <= '9')
                return character - '0';

            if (character >= 'a' && character <= 'f')
                return character - 'a' + 10;

            if (character >= 'A' && character <= 'F')
                return character - 'A' + 10;

            return -1;
        }

        inline std::string decodeLabel(const std::string &value)
        {
            std::string result;

            for (std::size_t index = 0; index < value.size(); ++index)
            {
                if (value[index] == '\\' && index + 3 < value.size() && value[index + 1] == 'x')
                {
                    const int high = hexDigit(value[index + 2]);
                    const int low = hexDigit(value[index + 3]);

                    if (high >= 0 && low >= 0)
                    {
                        result.push_back(
                            static_cast<char>(high * 16 + low));

                        index += 3;
                        continue;
                    }
                }

                result.push_back(value[index]);
            }

            return result;
        }

        inline std::map<std::string, std::string> labels()
        {
            std::map<std::string, std::string> result;

            std::error_code error;
            fs::directory_iterator entry(
                "/dev/disk/by-label", error);
            const fs::directory_iterator end;

            while (!error && entry != end)
            {
                struct stat device{};

                if (::stat(entry->path().c_str(), &device) == 0 && S_ISBLK(device.st_mode))
                {
                    result[deviceNumber(device.st_rdev)] =
                        decodeLabel(
                            entry->path().filename().string());
                }

                entry.increment(error);
            }

            return result;
        }

        inline bool networkFilesystem(const std::string &name)
        {
            return name == "nfs" || name == "nfs4" || name == "cifs" || name == "smb3" || name == "9p" || name == "drvfs" || name == "virtiofs";
        }

        inline void capacity(
            const std::string &mountpoint,
            DiskInfo &info)
        {
            struct statvfs statistics{};

            if (::statvfs(mountpoint.c_str(), &statistics) != 0)
                return;

            const std::uint64_t blockSize =
                statistics.f_frsize != 0
                    ? statistics.f_frsize
                    : statistics.f_bsize;

            info.totalBytes =
                bytes(statistics.f_blocks, blockSize);

            if (statistics.f_bfree <= statistics.f_blocks)
                info.freeBytes =
                    bytes(statistics.f_bfree, blockSize);

            if (info.totalBytes >= 0 && info.freeBytes >= 0)
                info.usedBytes =
                    info.totalBytes - info.freeBytes;

            // These are inode counts, not directory traversal counts.
            if (statistics.f_files > 0 && statistics.f_files !=
                                              static_cast<fsfilcnt_t>(-1))
            {
                info.totalFiles =
                    signedCount(statistics.f_files);

                if (statistics.f_ffree <= statistics.f_files && statistics.f_ffree !=
                                                                    static_cast<fsfilcnt_t>(-1))
                {
                    info.usedFiles = signedCount(
                        statistics.f_files - statistics.f_ffree);
                }
            }
        }
    }

    inline std::vector<DiskInfo> getDiskInfo()
    {
        namespace fs = std::filesystem;

        std::vector<DiskInfo> result;
        const auto labels = disk_detail::labels();

        std::ifstream mounts("/proc/self/mountinfo");
        std::string line;

        while (std::getline(mounts, line))
        {
            std::istringstream fields(line);

            std::string id, parent, device, root, mountpoint, options;

            if (!(fields >> id >> parent >> device >> root >> mountpoint >> options))
                continue;

            std::string field;

            while (fields >> field && field != "-")
            {
                // Skip optional mount fields.
            }

            std::string filesystem, source;

            if (field != "-" || !(fields >> filesystem >> source))
                continue;

            mountpoint = disk_detail::unescapeMount(mountpoint);
            source = disk_detail::unescapeMount(source);

            // Some filesystems report a synthetic mount device number.
            std::error_code error;
            fs::path block =
                fs::path("/sys/dev/block") / device;

            bool hasBlock = fs::exists(block, error);

            if (!hasBlock && source.starts_with("/dev/"))
            {
                struct stat sourceDevice{};

                if (::stat(source.c_str(), &sourceDevice) == 0 && S_ISBLK(sourceDevice.st_mode))
                {
                    device =
                        disk_detail::deviceNumber(
                            sourceDevice.st_rdev);

                    block = fs::path("/sys/dev/block") / device;
                    error.clear();
                    hasBlock = fs::exists(block, error);
                }
            }

            const bool network =
                disk_detail::networkFilesystem(filesystem);

            // Exclude proc, sysfs, tmpfs, and similar pseudo-filesystems.
            if (!hasBlock && !network && filesystem != "zfs")
                continue;

            DiskInfo info;
            info.filesystem = filesystem;
            info.mountpoint = mountpoint;
            info.mountFrom = source;

            info.volumeType.emplace_back(
                network ? "Network" : "Regular");

            if (("," + options + ",").find(",ro,") != std::string::npos)
                info.volumeType.emplace_back("ReadOnly");

            if (root != "/")
                info.volumeType.emplace_back("Subtree");

            if (const auto label = labels.find(device);
                label != labels.end())
            {
                info.name = label->second;
            }

            // Btrfs and ZFS can span devices outside the slaves graph.
            if (hasBlock && filesystem != "btrfs" && filesystem != "zfs")
            {
                info.type = disk_detail::medium(block);
            }

            disk_detail::capacity(mountpoint, info);
            result.push_back(std::move(info));
        }

        std::sort(
            result.begin(), result.end(),
            [](const DiskInfo &left, const DiskInfo &right)
            {
                return left.mountpoint < right.mountpoint;
            });

        return result;
    }
}