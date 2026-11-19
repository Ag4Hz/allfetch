#include "allfetch/render/render.hpp"
#include <nlohmann/json.hpp>
#include <ostream>
#include <string>
#include <type_traits>

namespace
{
    nlohmann::json valueOrNull(const std::string &value)
    {
        if (value == "Unknown")
            return nullptr;

        return value;
    }

    template <typename T>
        requires(
            std::is_arithmetic_v<T> &&
            !std::is_same_v<T, bool>)
    nlohmann::json valueOrNull(
        T value,
        T unknown = static_cast<T>(-1))
    {
        if (value == unknown)
            return nullptr;

        return value;
    }
}

namespace allfetch::system
{
    void to_json(nlohmann::json &json, const OSInfo &info)
    {
        json = {
            {"name", valueOrNull(info.name)},
            {"version", valueOrNull(info.version)},
            {"kernelName", valueOrNull(info.kernelName)},
            {"kernelVersion", valueOrNull(info.kernelVersion)},
            {"architecture", valueOrNull(info.architecture)},
        };
    }

    void to_json(nlohmann::json &json, const TerminalInfo &info)
    {
        json = {
            {"name", valueOrNull(info.name)},
            {"version", valueOrNull(info.version)},
            {"font", valueOrNull(info.font)},
            {"fontSource", valueOrNull(info.fontSource)},
        };
    }

    void to_json(nlohmann::json &json, const ShellInfo &info)
    {
        json = {
            {"name", valueOrNull(info.name)},
            {"executable", valueOrNull(info.executable)},
            {"version", valueOrNull(info.version)},
        };
    }

    void to_json(nlohmann::json &json, const UptimeInfo &info)
    {
        json = {{"seconds", valueOrNull(info.seconds)}};
    }

    void to_json(nlohmann::json &json, const MemoryInfo &info)
    {
        json = {
            {"totalPhysicalMemory", valueOrNull(info.totalPhysicalMemory)},
            {"availablePhysicalMemory", valueOrNull(info.availablePhysicalMemory)},
            {"totalVirtualMemory", valueOrNull(info.totalVirtualMemory)},
            {"availableVirtualMemory", valueOrNull(info.availableVirtualMemory)},
        };
    }

    void to_json(nlohmann::json &json, const CPUInfo &info)
    {
        json = {
            {"model", valueOrNull(info.model)},
            {"vendor", valueOrNull(info.vendor)},
            {"architecture", valueOrNull(info.architecture)},
            {"physicalCores", valueOrNull(info.physicalCores)},
            {"logicalCores", valueOrNull(info.logicalCores)},
            {"onlineCores", valueOrNull(info.onlineCores)},
            {"reportedFrequency", valueOrNull(info.reportedFrequency)},
            {"baseFrequency", valueOrNull(info.baseFrequency)},
            {"maxFrequency", valueOrNull(info.maxFrequency)},
        };
    }

    void to_json(nlohmann::json &json, const GPUInfo &info)
    {
        json = {
            {"model", valueOrNull(info.model)},
            {"vendor", valueOrNull(info.vendor)},
            {"driverVersion", valueOrNull(info.driverVersion)},
            {"type", valueOrNull(info.type)},
            {"deviceId", valueOrNull(info.deviceId)},
            {"memorySizeMB", valueOrNull(info.memorySizeMB)},
            {"sharedMemorySizeMB", valueOrNull(info.sharedMemorySizeMB)},
        };
    }

    void to_json(nlohmann::json &json, const SwapInfo &info)
    {
        json = {
            {"totalBytes", valueOrNull(info.totalBytes)},
            {"usedBytes", valueOrNull(info.usedBytes)},
        };
    }

    void to_json(nlohmann::json &json, const LocalIPInfo &info)
    {
        json = {
            {"interfaceName", valueOrNull(info.interfaceName)},
            {"address", valueOrNull(info.address)},
            {"prefixLength", valueOrNull(info.prefixLength)},
        };
    }

    void to_json(nlohmann::json &json, const HostInfo &info)
    {
        json = {
            {"family", valueOrNull(info.family)},
            {"name", valueOrNull(info.name)},
            {"version", valueOrNull(info.version)},
            {"sku", valueOrNull(info.sku)},
            {"vendor", valueOrNull(info.vendor)},
            {"serial", valueOrNull(info.serial)},
            {"uuid", valueOrNull(info.uuid)},
        };
    }

    void to_json(nlohmann::json &json, const DisplayInfo &info)
    {
        json = {
            {"name", valueOrNull(info.name)},
            {"drrStatus", valueOrNull(info.drrStatus)},
            {"hdrStatus", valueOrNull(info.hdrStatus)},
            {"type", valueOrNull(info.type)},
            {"serial", valueOrNull(info.serial)},
            {"platformApi", valueOrNull(info.platformApi)},
            {"primary", info.primaryKnown ? nlohmann::json(info.primary) : nlohmann::json(nullptr)},
            {"primaryKnown", info.primaryKnown},
            {"id", valueOrNull(info.id)},
            {"width", valueOrNull(info.width)},
            {"height", valueOrNull(info.height)},
            {"effectiveDpi", valueOrNull(info.effectiveDpi)},
            {"scaledWidth", valueOrNull(info.scaledWidth)},
            {"scaledHeight", valueOrNull(info.scaledHeight)},
            {"preferredWidth", valueOrNull(info.preferredWidth)},
            {"preferredHeight", valueOrNull(info.preferredHeight)},
            {"physicalWidthMm", valueOrNull(info.physicalWidthMm)},
            {"physicalHeightMm", valueOrNull(info.physicalHeightMm)},
            {"rotationDegrees", valueOrNull(info.rotationDegrees)},
            {"bitsPerColorChannel", valueOrNull(info.bitsPerColorChannel)},
            {"manufactureYear", valueOrNull(info.manufactureYear)},
            {"manufactureWeek", valueOrNull(info.manufactureWeek)},
            {"refreshRateHz", valueOrNull(info.refreshRateHz)},
            {"preferredRefreshRateHz", valueOrNull(info.preferredRefreshRateHz)},
        };
    }

    void to_json(nlohmann::json &json, const DiskInfo &info)
    {
        json = {
            {"filesystem", valueOrNull(info.filesystem)},
            {"mountpoint", valueOrNull(info.mountpoint)},
            {"mountFrom", valueOrNull(info.mountFrom)},
            {"type", valueOrNull(info.type)},
            {"name", valueOrNull(info.name)},
            {"volumeType", info.volumeType},
            {"totalBytes", valueOrNull(info.totalBytes)},
            {"freeBytes", valueOrNull(info.freeBytes)},
            {"usedBytes", valueOrNull(info.usedBytes)},
            {"totalFiles", valueOrNull(info.totalFiles)},
            {"usedFiles", valueOrNull(info.usedFiles)},
            {"creationTimeMs", valueOrNull(info.creationTimeMs)},
        };
    }

    void to_json(nlohmann::json &json, const BatteryInfo &info)
    {
        json = {
            {"manufacturer", valueOrNull(info.manufacturer)},
            {"manufactureDate", valueOrNull(info.manufactureDate)},
            {"modelName", valueOrNull(info.modelName)},
            {"technology", valueOrNull(info.technology)},
            {"serial", valueOrNull(info.serial)},
            {"status", info.status},
            {"timeRemainingSeconds", valueOrNull(info.timeRemainingSeconds)},
            {"cycleCount", valueOrNull(info.cycleCount)},
            {"capacityPercent", valueOrNull(info.capacityPercent)},
            {"temperatureCelsius", valueOrNull(info.temperatureCelsius, -999.0)},
        };
    }

    void to_json(nlohmann::json &json, const LocaleInfo &info)
    {
        json = {{"name", valueOrNull(info.name)}};
    }

    void to_json(nlohmann::json &json, const ColorsInfo &info)
    {
        auto hexValues = nlohmann::json::array();
        for (const auto &value : info.hexValues)
            hexValues.push_back(value.empty() ? nlohmann::json(nullptr) : valueOrNull(value));

        json = {{"hexValues", hexValues}};
    }

    void to_json(nlohmann::json &json, const TitleInfo &info)
    {
        json = {
            {"userId", valueOrNull(info.userId)},
            {"userName", valueOrNull(info.userName)},
            {"fullUserName", valueOrNull(info.fullUserName)},
            {"hostName", valueOrNull(info.hostName)},
            {"homeDir", valueOrNull(info.homeDir)},
            {"exePath", valueOrNull(info.exePath)},
            {"userShell", valueOrNull(info.userShell)},
            {"cwd", valueOrNull(info.cwd)},
            {"pid", valueOrNull(info.pid)},
        };
    }

    void to_json(nlohmann::json &json, const WindowManagerInfo &info)
    {
        json = {
            {"name", valueOrNull(info.name)},
            {"theme", valueOrNull(info.theme)},
        };
    }

    void to_json(nlohmann::json &json, const DesktopAppearanceInfo &info)
    {
        json = {
            {"theme", valueOrNull(info.theme)},
            {"colorScheme", valueOrNull(info.colorScheme)},
            {"iconTheme", valueOrNull(info.iconTheme)},
            {"cursorTheme", valueOrNull(info.cursorTheme)},
            {"font", valueOrNull(info.font)},
            {"desktopIcons", info.desktopIconsKnown ? nlohmann::json(info.desktopIcons) : nlohmann::json(nullptr)},
            {"desktopIconsKnown", info.desktopIconsKnown},
            {"cursorSize", valueOrNull(info.cursorSize)},
        };
    }

    void to_json(nlohmann::json &json, const SystemInfo &info)
    {
        json = {
            {"osInfo", info.osInfo},
            {"hostInfo", info.hostInfo},
            {"memoryInfo", info.memoryInfo},
            {"cpuInfo", info.cpuInfo},
            {"gpuInfo", info.gpuInfo},
            {"terminalInfo", info.terminalInfo},
            {"shellInfo", info.shellInfo},
            {"uptimeInfo", info.uptimeInfo},
            {"swapInfo", info.swapInfo},
            {"localIPInfo", info.localIPInfo},
            {"displayInfo", info.displayInfo},
            {"diskInfo", info.diskInfo},
            {"batteryInfo", info.batteryInfo},
            {"localeInfo", info.localeInfo},
            {"colorsInfo", info.colorsInfo},
            {"titleInfo", info.titleInfo},
            {"windowManagerInfo", info.windowManagerInfo},
            {"desktopAppearanceInfo", info.desktopAppearanceInfo},
        };
    }
}

namespace allfetch::render
{
    void renderJson(
        const system::SystemInfo &info,
        std::ostream &output)
    {
        const nlohmann::json document = {
            {"schemaVersion", 1},
            {"systemInfo", info}};

        output << document.dump(2) << "\n";
    }
}
