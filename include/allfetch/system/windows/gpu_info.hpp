#pragma once

#include <allfetch/system/system_info.hpp>
#include "allfetch/system/windows/platform.hpp"
#include <dxgi.h>
#include <directx/dxcore.h>
#include <array>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace allfetch::system
{
    namespace gpu_detail
    {
        // The MinGW headers do not provide d3dkmthk.h. These are the
        // Windows ABI layouts for the adapter and node queries used below.
        struct OpenAdapter
        {
            LUID luid;
            UINT handle;
        };
        struct CloseAdapter
        {
            UINT handle;
        };
        struct AdapterQuery
        {
            UINT handle;
            UINT type;
            void *data;
            UINT size;
        };
        struct NodeMetadata
        {
            UINT ordinal;
            UINT engineType;
            std::array<unsigned char, 70> remaining;
        };
        struct alignas(8) NodePerformance
        {
            UINT ordinal;
            UINT physicalAdapterIndex;
            ULONGLONG frequency;
            ULONGLONG maxFrequency;
            ULONGLONG maxFrequencyOverclocked;
            ULONG voltage;
            ULONG maxVoltage;
            ULONG maxVoltageOverclocked;
            alignas(8) ULONGLONG transitionLatency;
            ULONGLONG reserved;
        };

        inline double maxGraphicsClockMHz(const LUID &luid)
        {
            std::unique_ptr<std::remove_pointer_t<HMODULE>, decltype(&FreeLibrary)> gdi(
                LoadLibraryExW(L"gdi32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32), &FreeLibrary);
            if (!gdi)
                return -1;

            const auto open = reinterpret_cast<LONG(WINAPI *)(OpenAdapter *)>(
                GetProcAddress(gdi.get(), "D3DKMTOpenAdapterFromLuid"));
            const auto query = reinterpret_cast<LONG(WINAPI *)(const AdapterQuery *)>(
                GetProcAddress(gdi.get(), "D3DKMTQueryAdapterInfo"));
            const auto close = reinterpret_cast<LONG(WINAPI *)(const CloseAdapter *)>(
                GetProcAddress(gdi.get(), "D3DKMTCloseAdapter"));
            OpenAdapter adapter{luid, 0};
            if (!open || !query || !close || open(&adapter) < 0)
                return -1;

            constexpr UINT nodeMetadataQuery = 25;
            constexpr UINT nodePerformanceQuery = 61;
            constexpr UINT graphicsEngine = 1;
            double frequency = -1;
            for (UINT node = 0; node < 64; ++node)
            {
                NodeMetadata metadata{};
                metadata.ordinal = node;
                // Node metadata's ABI payload ends before the C++ tail padding.
                AdapterQuery request{adapter.handle, nodeMetadataQuery, &metadata, 78};
                if (query(&request) < 0)
                    break;
                if (metadata.engineType != graphicsEngine)
                    continue;

                NodePerformance performance{};
                performance.ordinal = node;
                static_assert(sizeof(NodePerformance) == 64);
                request = {adapter.handle, nodePerformanceQuery, &performance, sizeof(performance)};
                LONG status = query(&request);
                if (status < 0)
                {
                    request.size -= sizeof(performance.reserved);
                    status = query(&request);
                }
                if (status >= 0 && performance.maxFrequency > 0)
                {
                    frequency = performance.maxFrequency / 1'000'000.0;
                    break;
                }
            }
            const CloseAdapter opened{adapter.handle};
            close(&opened);
            return frequency;
        }

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

        inline std::string adapterType(IDXCoreAdapterFactory *factory, const LUID &luid)
        {
            if (!factory)
                return "Unknown";
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

        IDXCoreAdapterFactory *core = nullptr;
        const HRESULT coreStatus = DXCoreCreateAdapterFactory(
            gpu_detail::adapterFactoryId, reinterpret_cast<void **>(&core));
        std::unique_ptr<IDXCoreAdapterFactory, gpu_detail::ReleaseInterface>
            coreFactory(SUCCEEDED(coreStatus) ? core : nullptr);

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

            gpu.type = gpu_detail::adapterType(coreFactory.get(), description.AdapterLuid);
            gpu.model = windows_platform::utf8OrUnknown(description.Description);
            gpu.vendor = gpu_detail::vendorName(description.VendorId);
            gpu.deviceId = static_cast<int>(description.DeviceId);
            gpu.maxFrequencyMHz = gpu_detail::maxGraphicsClockMHz(description.AdapterLuid);

            gpu.memorySizeMiB = static_cast<int>(
                description.DedicatedVideoMemory / (1024ULL * 1024ULL));

            gpu.sharedMemorySizeMiB = static_cast<int>(
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
