#pragma once

#include "allfetch/system/system_info.hpp"
#include <string>
#include <fstream>
#include <sys/utsname.h>

inline allfetch::system::OSInfo getOSInfo()
{
    allfetch::system::OSInfo linux;
    linux.name = "Linux";
    linux.version = "Unknown";
    linux.kernelName = "Unknown";
    linux.kernelVersion = "Unknown";
    linux.architecture = "Unknown";

    struct utsname buffer;
    if (uname(&buffer) == 0)
    {
        linux.kernelName = buffer.sysname;
        linux.kernelVersion = buffer.release;
        linux.architecture = buffer.machine;
    }

    std::ifstream file("/etc/os-release");
    if (!file.is_open())
    {
        file.open("/usr/lib/os-release");
    }

    std::string line;
    std::string versionId;
    while (std::getline(file, line))
    {
        const auto sep = line.find('=');
        if (sep == std::string::npos) continue;
        auto value = line.substr(sep + 1);
        if (value.size() >= 2 && (value.front() == '"' || value.front() == '\'') && value.back() == value.front())
            value = value.substr(1, value.size() - 2);
        if (value.empty()) continue;
        if (line.starts_with("NAME=")) linux.name = value;
        if (line.starts_with("VERSION=")) linux.version = value;
        if (line.starts_with("VERSION_ID=")) versionId = value;
    }

    if (linux.version == "Unknown" && !versionId.empty()) linux.version = versionId;

    return linux;
}
