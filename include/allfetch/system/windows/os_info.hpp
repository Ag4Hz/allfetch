#pragma once

#include "allfetch/system/system_info.hpp"
#include <string>
#include <windows.h>
#include <optional>
#include <memory>
#include "allfetch/system/windows/platform.hpp"

namespace
{

    /* Function to get the Windows version information
    Output:
        std::optional<std::wstring>: The Windows version information, or std::nullopt if it cannot be determined
    */
    std::optional<std::wstring> getWindowsName()
    {
        HMODULE library = LoadLibraryExW(L"winbrand.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);

        if (!library)
        {
            return std::nullopt;
        }

        // Get the function pointer for the BrandingFormatString function from the winbrand.dll library
        using BrandingFunction = PWSTR(WINAPI *)(PCWSTR);

        const auto brandingFormatString =
            reinterpret_cast<BrandingFunction>(
                GetProcAddress(library, "BrandingFormatString"));

        if (!brandingFormatString)
        {
            FreeLibrary(library);
            return std::nullopt;
        }

        std::unique_ptr<wchar_t, decltype(&GlobalFree)> text(
            brandingFormatString(L"%WINDOWS_LONG%"), &GlobalFree);
        FreeLibrary(library);
        if (!text || !*text) return std::nullopt;
        return std::wstring(text.get());
    }

    // Function to get the Windows release information from the registry
    constexpr wchar_t windowsVersionKey[] = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
    std::optional<std::wstring> getWindowsRelease()
    {
        wchar_t buffer[256]{};
        DWORD bytesRead = sizeof(buffer);

        const LSTATUS status = RegGetValueW(
            HKEY_LOCAL_MACHINE,
            windowsVersionKey,
            L"DisplayVersion",
            RRF_RT_REG_SZ | RRF_NOEXPAND,
            nullptr,
            buffer,
            &bytesRead);

        if (status == ERROR_SUCCESS)
        {
            return std::make_optional<std::wstring>(buffer);
        }
        return std::nullopt;
    }

    // Function to get the Windows revision information from the registry
    std::optional<DWORD> getWindowsRevision()
    {
        DWORD revision{};
        DWORD bytesRead = sizeof(revision);

        const LSTATUS status = RegGetValueW(
            HKEY_LOCAL_MACHINE,
            windowsVersionKey,
            L"UBR",
            RRF_RT_REG_DWORD | RRF_NOEXPAND,
            nullptr,
            &revision,
            &bytesRead);

        if (status == ERROR_SUCCESS)
        {
            return std::make_optional<DWORD>(revision);
        }
        return std::nullopt;
    }

    // Function to get the Windows kernel version information from the registry
    std::optional<std::string> getWindowsKernelVersion()
    {
        const HMODULE module = GetModuleHandleW(L"ntdll.dll");

        if (!module)
        {
            return std::nullopt;
        }

        using VersionFunction = LONG(WINAPI *)(OSVERSIONINFOEXW *);

        const auto rtlGetVersion =
            reinterpret_cast<VersionFunction>(
                GetProcAddress(module, "RtlGetVersion"));

        if (!rtlGetVersion)
        {
            return std::nullopt;
        }

        OSVERSIONINFOEXW osvi{};
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);

        if (rtlGetVersion(&osvi) == 0)
        {
            std::string version =
                std::to_string(osvi.dwMajorVersion) + "." +
                std::to_string(osvi.dwMinorVersion) + "." +
                std::to_string(osvi.dwBuildNumber);

            if (const auto revision = getWindowsRevision())
            {
                version += "." + std::to_string(*revision);
            }

            return version;
        }

        return std::nullopt;
    }

}

inline allfetch::system::OSInfo getOSInfo()
{
    allfetch::system::OSInfo windows;
    windows.name = "Windows";
    windows.version = "Unknown";
    windows.kernelName = "WIN32_NT";
    windows.kernelVersion = "Unknown";
    windows.architecture = "Unknown";

    if (const auto windowsName = getWindowsName())
    {
        auto value = allfetch::system::windows_platform::utf8(*windowsName);
        if (!value.empty()) windows.name = value;
    }
    if (const auto windowsRelease = getWindowsRelease())
    {
        auto value = allfetch::system::windows_platform::utf8(*windowsRelease);
        if (!value.empty()) windows.version = value;
    }
    if (const auto windowsKernelVersion = getWindowsKernelVersion())
    {
        windows.kernelVersion = *windowsKernelVersion;
    }
    windows.architecture = allfetch::system::windows_platform::architecture();

    return windows;
}
