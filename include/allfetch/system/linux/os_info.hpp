#pragma once

#include <string>
#include <fstream>
#include <sys/utsname.h>

struct Linux
{
    std::string name;
    std::string version;
    std::string kernelName;
    std::string kernelVersion;
    std::string architecture;
};

Linux getLinuxInfo()
{
    Linux linux;
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
    std::string wholeName;
    while (std::getline(file, line))
    {
        if (line.find("PRETTY_NAME=") == 0)
        {
            wholeName = line.substr(13, line.size() - 14); // Remove quotes
        }
    }

    linux.name = wholeName.substr(0, wholeName.find(" "));
    linux.version = wholeName.substr(wholeName.find(" ") + 1);

    return linux;
}