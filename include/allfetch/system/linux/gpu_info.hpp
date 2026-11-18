#pragma once

#include "allfetch/system/system_info.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace allfetch::system::linux_gpu_fallback
{
    namespace fs = std::filesystem;

    inline std::optional<unsigned int> readHex(const fs::path &path)
    {
        std::ifstream file(path);
        unsigned int value = 0;

        if (file >> std::hex >> value)
        {
            return value;
        }

        return std::nullopt;
    }

    inline std::string vendorName(unsigned int id)
    {
        switch (id)
        {
        case 0x10de: return "NVIDIA";
        case 0x1002: return "AMD";
        case 0x8086: return "Intel";
        case 0x1414: return "Microsoft";
        case 0x14e4: return "Broadcom";
        default: return "Unknown";
        }
    }

    inline std::vector<GPUInfo> collect(
        const fs::path &pciRoot = "/sys/bus/pci/devices",
        const fs::path &drmRoot = "/sys/class/drm")
    {
        std::vector<GPUInfo> devices;
        std::set<fs::path> visited;

        const auto addDevice = [&](const fs::path &path)
        {
            std::error_code error;
            const auto device = fs::canonical(path, error);

            if (error || !visited.insert(device).second)
            {
                return;
            }

            const auto vendor = readHex(device / "vendor");
            const auto id = readHex(device / "device");
            const auto driverPath = fs::read_symlink(device / "driver", error);
            const std::string driver = error ? "" : driverPath.filename().string();
            GPUInfo gpu;

            if (vendor && id)
            {
                gpu.vendor = vendorName(*vendor);

                if (*id <= 0xffffu)
                {
                    gpu.deviceId = static_cast<int>(*id);
                }

                std::ostringstream model;
                model << gpu.vendor << " PCI display device "
                      << std::hex << std::setfill('0')
                      << std::setw(4) << *vendor << ':'
                      << std::setw(4) << *id;
                gpu.model = model.str();
            }
            else if (driver == "v3d" || driver == "vc4")
            {
                gpu.model = driver == "v3d"
                    ? "Broadcom V3D"
                    : "Broadcom VideoCore (vc4)";
                gpu.vendor = "Broadcom";
                gpu.type = "Integrated";
            }
            else
            {
                gpu.model = driver.empty()
                    ? "DRM render device"
                    : "DRM render device (" + driver + ")";
            }

            // A module version is not exposed by every driver.
            std::ifstream version(device / "driver/module/version");
            std::string text;

            if (std::getline(version, text) && !text.empty())
            {
                gpu.driverVersion = text;
            }

            devices.push_back(gpu);
        };

        std::error_code error;
        fs::directory_iterator entry(pciRoot, error);
        const fs::directory_iterator end;

        while (!error && entry != end)
        {
            const auto deviceClass = readHex(entry->path() / "class");

            if (deviceClass && (*deviceClass >> 16) == 0x03)
            {
                addDevice(entry->path());
            }

            entry.increment(error);
        }

        error.clear();
        entry = fs::directory_iterator(drmRoot, error);

        while (!error && entry != end)
        {
            const auto name = entry->path().filename().string();

            // Ignore display connector entries such as card0-HDMI-A-1.
            if (name.starts_with("renderD"))
            {
                addDevice(entry->path() / "device");
            }

            entry.increment(error);
        }

        return devices;
    }
}

#if defined(ALLFETCH_HAS_VULKAN)
#include <vulkan/vulkan.h>

#include <cstdint>
#include <limits>
#include <memory>

namespace allfetch::system
{
    namespace linux_gpu_detail
    {
        struct DestroyInstance
        {
            void operator()(VkInstance instance) const noexcept
            {
                if (instance != VK_NULL_HANDLE)
                {
                    vkDestroyInstance(instance, nullptr);
                }
            }
        };

        inline std::string vendorName(std::uint32_t id)
        {
            switch (id)
            {
            case 0x10DE:
                return "NVIDIA";
            case 0x1002:
            case 0x1022:
                return "AMD";
            case 0x8086:
                return "Intel";
            default:
                return "Unknown";
            }
        }
    }

    /* Function to get the GPU information of the system
    Output:
        GPUInfo: The GPU information of the system
    */
    inline std::vector<GPUInfo> getVulkanGPUInfo()
    {
        std::vector<GPUInfo> gpuInfoList;

        VkApplicationInfo application{};
        application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        application.pApplicationName = "allfetch";
        application.apiVersion = VK_API_VERSION_1_1;

        VkInstanceCreateInfo creation{};
        creation.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        creation.pApplicationInfo = &application;

        VkInstance rawInstance = VK_NULL_HANDLE;

        if (vkCreateInstance(
                &creation, nullptr, &rawInstance) != VK_SUCCESS)
        {
            return gpuInfoList;
        }

        std::unique_ptr<VkInstance_T, linux_gpu_detail::DestroyInstance>
            instance(rawInstance);

        std::uint32_t count = 0;

        if (vkEnumeratePhysicalDevices(
                instance.get(), &count, nullptr) != VK_SUCCESS ||
            count == 0)
        {
            return gpuInfoList;
        }

        std::vector<VkPhysicalDevice> devices;

        // The number of devices can change between enumeration calls.
        for (;;)
        {
            devices.resize(count);

            const VkResult status = vkEnumeratePhysicalDevices(
                instance.get(), &count, devices.data());

            if (status == VK_SUCCESS)
            {
                devices.resize(count);
                break;
            }

            if (status != VK_INCOMPLETE)
            {
                return gpuInfoList;
            }

            if (vkEnumeratePhysicalDevices(
                    instance.get(), &count, nullptr) != VK_SUCCESS)
            {
                return gpuInfoList;
            }
        }

        for (const auto device : devices)
        {
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(device, &properties);

            // Skip software rendering and non-GPU device types.
            if (properties.deviceType !=
                    VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU &&
                properties.deviceType !=
                    VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
                properties.deviceType !=
                    VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)
            {
                continue;
            }

            GPUInfo gpu;

            gpu.model = properties.deviceName;
            gpu.vendor = linux_gpu_detail::vendorName(
                properties.vendorID);

            if (properties.deviceID <=
                static_cast<std::uint32_t>(
                    std::numeric_limits<int>::max()))
            {
                gpu.deviceId = static_cast<int>(properties.deviceID);
            }

            switch (properties.deviceType)
            {
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                gpu.type = "Integrated";
                break;
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                gpu.type = "Discrete";
                break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                gpu.type = "Virtual";
                break;
            default:
                break;
            }

            // Local heaps on an integrated GPU can be system RAM.
            // Only report this total as dedicated memory for a dGPU.
            if (properties.deviceType ==
                VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            {
                VkPhysicalDeviceMemoryProperties memory{};
                vkGetPhysicalDeviceMemoryProperties(device, &memory);

                VkDeviceSize localBytes = 0;

                for (std::uint32_t index = 0;
                     index < memory.memoryHeapCount;
                     ++index)
                {
                    const auto &heap = memory.memoryHeaps[index];

                    if (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
                    {
                        localBytes += heap.size;
                    }
                }

                const auto mib = localBytes / (1024ULL * 1024ULL);

                if (localBytes != 0 &&
                    mib <= static_cast<VkDeviceSize>(
                               std::numeric_limits<int>::max()))
                {
                    gpu.memorySizeMB = static_cast<int>(mib);
                }
            }

            gpuInfoList.push_back(gpu);
        }

        return gpuInfoList;
    }
}

#endif

namespace allfetch::system
{
    inline std::vector<GPUInfo> getGPUInfo()
    {
#if defined(ALLFETCH_HAS_VULKAN)
        auto devices = getVulkanGPUInfo();

        if (!devices.empty())
        {
            return devices;
        }
#endif

        return linux_gpu_fallback::collect();
    }
}
