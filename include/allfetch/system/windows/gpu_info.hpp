#pragma once

#include <allfetch/system/system_info.hpp>
#include <windows.h>
#include <dxgi.h>
#include <directx/dxcore.h>
#include <memory>
#include <string>
#include <vector>

namespace allfetch::system
{
    namespace gpu_detail
    {
        struct ReleaseInterface
        {
            template <typename T>
            void operator()(T *object) const noexcept
            {
                if (object != nullptr)
                {
                    object->Release();
                }
            }
        };

        inline constexpr GUID adapterFactoryId{
            0x78ee5945, 0xc36e, 0x4b13, {0xa6, 0x69, 0x00, 0x5d, 0xd1, 0x1c, 0x0f, 0x06}};

        inline constexpr GUID adapterId{
            0xf0db4c7f, 0xfe5a, 0x42a2, {0xbd, 0x62, 0xf2, 0xa6, 0xcf, 0x6f, 0xc8, 0x3e}};

        inline std::string adapterType(const LUID &luid)
        {
            IDXCoreAdapterFactory *rawFactory = nullptr;

            if (FAILED(DXCoreCreateAdapterFactory(
                    adapterFactoryId,
                    reinterpret_cast<void **>(&rawFactory))))
            {
                return "Unknown";
            }

            std::unique_ptr<IDXCoreAdapterFactory, ReleaseInterface>
                factory(rawFactory);

            IDXCoreAdapter *rawAdapter = nullptr;

            if (FAILED(factory->GetAdapterByLuid(
                    luid,
                    adapterId,
                    reinterpret_cast<void **>(&rawAdapter))))
            {
                return "Unknown";
            }

            std::unique_ptr<IDXCoreAdapter, ReleaseInterface>
                adapter(rawAdapter);

            constexpr auto property = DXCoreAdapterProperty::IsIntegrated;

            if (!adapter->IsPropertySupported(property))
            {
                return "Unknown";
            }

            bool integrated = false;

            if (FAILED(adapter->GetProperty(
                    property, sizeof(integrated), &integrated)))
            {
                return "Unknown";
            }

            return integrated ? "Integrated" : "Discrete";
        }

        inline std::string toUtf8(const wchar_t *text)
        {
            const int required = WideCharToMultiByte(
                CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);

            if (required <= 1)
            {
                return "Unknown";
            }

            std::string result(required, '\0');

            if (WideCharToMultiByte(
                    CP_UTF8, 0, text, -1,
                    result.data(), required, nullptr, nullptr) == 0)
            {
                return "Unknown";
            }

            result.pop_back();
            return result;
        }

        inline std::string vendorName(UINT id)
        {
            switch (id)
            {
            case 0x10DE:
                return "NVIDIA";
            case 0x1002:
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
    inline std::vector<GPUInfo> getGPUInfo()
    {
        std::vector<GPUInfo> gpuInfoList;

        IDXGIFactory1 *rawFactory = nullptr;

        if (FAILED(CreateDXGIFactory1(
                IID_IDXGIFactory1,
                reinterpret_cast<void **>(&rawFactory))))
        {
            return gpuInfoList;
        }

        std::unique_ptr<IDXGIFactory1, gpu_detail::ReleaseInterface>
            factory(rawFactory);

        for (UINT index = 0;; ++index)
        {
            IDXGIAdapter1 *rawAdapter = nullptr;

            const HRESULT status = factory->EnumAdapters1(
                index, &rawAdapter);

            if (status == DXGI_ERROR_NOT_FOUND)
            {
                break;
            }

            if (FAILED(status))
            {
                break;
            }

            std::unique_ptr<IDXGIAdapter1, gpu_detail::ReleaseInterface>
                adapter(rawAdapter);

            DXGI_ADAPTER_DESC1 description{};

            if (FAILED(adapter->GetDesc1(&description)))
            {
                continue;
            }

            if (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                continue;
            }

            GPUInfo gpu;

            gpu.type = gpu_detail::adapterType(description.AdapterLuid);
            gpu.model = gpu_detail::toUtf8(description.Description);
            gpu.vendor = gpu_detail::vendorName(description.VendorId);
            gpu.deviceId = static_cast<int>(description.DeviceId);

            // These existing fields store MiB, despite their MB names.
            gpu.memorySizeMB = static_cast<int>(
                description.DedicatedVideoMemory / (1024ULL * 1024ULL));

            gpu.sharedMemorySizeMB = static_cast<int>(
                description.SharedSystemMemory / (1024ULL * 1024ULL));

            LARGE_INTEGER version{};

            if (SUCCEEDED(adapter->CheckInterfaceSupport(
                    IID_IDXGIDevice, &version)))
            {
                const auto high =
                    static_cast<unsigned long>(version.HighPart);
                const auto low = version.LowPart;

                gpu.driverVersion =
                    std::to_string((high >> 16) & 0xFFFFu) + "." +
                    std::to_string(high & 0xFFFFu) + "." +
                    std::to_string((low >> 16) & 0xFFFFu) + "." +
                    std::to_string(low & 0xFFFFu);
            }

            gpuInfoList.push_back(gpu);
        }

        return gpuInfoList;
    }
}