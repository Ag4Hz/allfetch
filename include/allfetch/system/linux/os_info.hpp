#pragma once

#include "allfetch/system/system_info.hpp"
#include <string>
#include <fstream>
#include <sstream>
#include <sys/utsname.h>

namespace allfetch::system::linux_os_detail
{
    inline void readOsRelease(std::istream &file, OSInfo &info)
    {
        std::string line;
        std::string versionId;
        while (std::getline(file, line))
        {
            if (line.ends_with('\r'))
                line.pop_back();

            const auto separator = line.find('=');
            if (separator == std::string::npos)
                continue;

            const auto key = line.substr(0, separator);
            auto value = line.substr(separator + 1);
            if (value.size() >= 2 &&
                (value.front() == '"' || value.front() == '\'') &&
                value.back() == value.front())
                value = value.substr(1, value.size() - 2);

            if (key == "ID")
                info.id = value;
            else if (key == "ID_LIKE")
            {
                info.idLike.clear();
                std::istringstream ids(value);
                for (std::string id; ids >> id;)
                    info.idLike.push_back(id);
            }
            else if (key == "NAME" && !value.empty())
                info.name = value;
            else if (key == "VERSION" && !value.empty())
                info.version = value;
            else if (key == "VERSION_ID" && !value.empty())
                versionId = value;
        }

        if (info.version == "Unknown" && !versionId.empty())
            info.version = versionId;
    }
}

inline allfetch::system::OSInfo getOSInfo()
{
    allfetch::system::OSInfo info;
    info.name = "Linux";

    struct utsname buffer;
    if (uname(&buffer) == 0)
    {
        info.kernelName = buffer.sysname;
        info.kernelVersion = buffer.release;
        info.architecture = buffer.machine;
    }

    std::ifstream file("/etc/os-release");
    if (!file.is_open())
    {
        file.open("/usr/lib/os-release");
    }

    allfetch::system::linux_os_detail::readOsRelease(file, info);
    return info;
}
