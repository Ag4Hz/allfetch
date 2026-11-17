#include "allfetch/core/argparser.hpp"
#include "allfetch/system/system_info.hpp"
#include <sys/utsname.h>
#include <string>
#include <fstream>

namespace allfetch::system
{

    /* Implementation of the getSystemInfo function
    Output:
        SystemInfo: The system information
    */
    SystemInfo getSystemInfo()
    {
        SystemInfo info;

        // Get the operating system information
        struct Linux
        {
            std::string name;
            std::string version;
            std::string kernelName;
            std::string kernelVersion;
            std::string architecture;
            Linux()
            {
                name = "Linux";
                version = "Unknown";
                kernelName = "Unknown";
                kernelVersion = "Unknown";
                architecture = "Unknown";

                struct utsname buffer;
                if (uname(&buffer) == 0)
                {
                    kernelName = buffer.sysname;
                    kernelVersion = buffer.release;
                    architecture = buffer.machine;
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

                name = wholeName.substr(0, wholeName.find(" "));
                version = wholeName.substr(wholeName.find(" ") + 1);
            }
        } linux;

        info.osInfo.name = linux.name;
        info.osInfo.version = linux.version;
        info.osInfo.kernelName = linux.kernelName;
        info.osInfo.kernelVersion = linux.kernelVersion;
        info.osInfo.architecture = linux.architecture;

        return info;
    }

}