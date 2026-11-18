#pragma once

#include "allfetch/system/system_info.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace allfetch::system
{
    namespace host_detail
    {
        inline std::string clean(std::string value)
        {
            const auto first = value.find_first_not_of(" \t\r\n");

            if (first == std::string::npos)
            {
                return "Unknown";
            }

            const auto last = value.find_last_not_of(" \t\r\n");
            value = value.substr(first, last - first + 1);

            if (value == "To be filled by O.E.M." ||
                value == "Default string" ||
                value == "Not Specified")
            {
                return "Unknown";
            }

            return value;
        }

#if defined(_WIN32)
        inline std::string formatUuid(
            const unsigned char *bytes,
            bool littleEndianFields)
        {
            const bool allZero = std::all_of(
                bytes, bytes + 16,
                [](unsigned char value)
                { return value == 0; });

            const bool allOnes = std::all_of(
                bytes, bytes + 16,
                [](unsigned char value)
                { return value == 0xff; });

            if (allZero || allOnes)
            {
                return "Unknown";
            }

            constexpr int littleEndianOrder[] = {
                3, 2, 1, 0, 5, 4, 7, 6,
                8, 9, 10, 11, 12, 13, 14, 15};

            std::ostringstream text;
            text << std::uppercase << std::hex << std::setfill('0');

            for (int index = 0; index < 16; ++index)
            {
                if (index == 4 || index == 6 ||
                    index == 8 || index == 10)
                {
                    text << '-';
                }

                const int source = littleEndianFields
                                       ? littleEndianOrder[index]
                                       : index;

                text << std::setw(2)
                     << static_cast<unsigned int>(bytes[source]);
            }

            return text.str();
        }

#elif defined(__linux__)
        inline std::string readText(const std::string &path)
        {
            std::ifstream file(path, std::ios::binary);
            std::string value;

            std::getline(file, value, '\0');
            return clean(value);
        }
#endif
    }

    inline HostInfo getHostInfo()
    {
        HostInfo info;

#if defined(_WIN32)
        constexpr DWORD provider = 0x52534D42; // 'RSMB'
        constexpr std::size_t wrapperSize = 8;

        const UINT required = GetSystemFirmwareTable(
            provider, 0, nullptr, 0);

        if (required < wrapperSize)
        {
            return info;
        }

        std::vector<unsigned char> buffer(required);

        const UINT received = GetSystemFirmwareTable(
            provider, 0, buffer.data(), required);

        if (received < wrapperSize || received > buffer.size())
        {
            return info;
        }

        DWORD tableLength = 0;
        std::memcpy(
            &tableLength, buffer.data() + 4, sizeof(tableLength));

        if (tableLength > received - wrapperSize)
        {
            return info;
        }

        const auto *table = buffer.data() + wrapperSize;
        const std::size_t end = tableLength;
        std::size_t position = 0;

        while (position < end && end - position >= 4)
        {
            const auto *record = table + position;
            const unsigned int type = record[0];
            const std::size_t length = record[1];

            if (length < 4 || length > end - position)
            {
                return info;
            }

            if (type == 127)
            {
                break;
            }

            const std::size_t stringsBegin = position + length;
            std::size_t stringsEnd = stringsBegin;

            while (stringsEnd < end && end - stringsEnd >= 2 &&
                   !(table[stringsEnd] == 0 &&
                     table[stringsEnd + 1] == 0))
            {
                ++stringsEnd;
            }

            if (stringsEnd >= end || end - stringsEnd < 2)
            {
                return info;
            }

            if (type == 1 && length >= 8)
            {
                const auto stringAt = [&](unsigned int index)
                    -> std::string
                {
                    if (index == 0)
                    {
                        return "Unknown";
                    }

                    std::size_t start = stringsBegin;

                    for (unsigned int current = 1;
                         start < stringsEnd;
                         ++current)
                    {
                        std::size_t stop = start;

                        while (stop < stringsEnd &&
                               table[stop] != 0)
                        {
                            ++stop;
                        }

                        if (current == index)
                        {
                            return host_detail::clean(std::string(
                                reinterpret_cast<const char *>(
                                    table + start),
                                stop - start));
                        }

                        start = stop + 1;
                    }

                    return "Unknown";
                };

                info.vendor = stringAt(record[0x04]);
                info.name = stringAt(record[0x05]);
                info.version = stringAt(record[0x06]);
                info.serial = stringAt(record[0x07]);

                if (length >= 0x18)
                {
                    const unsigned int major = buffer[1];
                    const unsigned int minor = buffer[2];

                    const bool littleEndianFields =
                        major > 2 || (major == 2 && minor >= 6);

                    info.uuid = host_detail::formatUuid(
                        record + 0x08, littleEndianFields);
                }

                if (length > 0x19)
                {
                    info.sku = stringAt(record[0x19]);
                }

                if (length > 0x1a)
                {
                    info.family = stringAt(record[0x1a]);
                }

                return info;
            }

            position = stringsEnd + 2;
        }

#elif defined(__linux__)
        const std::string dmi = "/sys/class/dmi/id/";

        info.family = host_detail::readText(dmi + "product_family");
        info.name = host_detail::readText(dmi + "product_name");
        info.version = host_detail::readText(dmi + "product_version");
        info.sku = host_detail::readText(dmi + "product_sku");
        info.vendor = host_detail::readText(dmi + "sys_vendor");
        info.serial = host_detail::readText(dmi + "product_serial");
        info.uuid = host_detail::readText(dmi + "product_uuid");

        // Raspberry Pi and other device-tree systems.
        const std::string tree = "/sys/firmware/devicetree/base/";

        if (info.name == "Unknown")
        {
            info.name = host_detail::readText(tree + "model");
        }

        if (info.serial == "Unknown")
        {
            info.serial = host_detail::readText(
                tree + "serial-number");
        }

        if (info.vendor == "Unknown")
        {
            std::ifstream compatible(
                tree + "compatible", std::ios::binary);

            std::string entry;

            while (std::getline(compatible, entry, '\0'))
            {
                if (entry.starts_with("raspberrypi,"))
                {
                    info.vendor = "Raspberry Pi";
                    break;
                }
            }
        }

        if (info.name == "Unknown" &&
            (std::getenv("WSL_INTEROP") != nullptr ||
             std::getenv("WSL_DISTRO_NAME") != nullptr))
        {
            info.name = "Windows Subsystem for Linux";
            info.vendor = "Microsoft";
        }
#endif

        return info;
    }
}