#pragma once

#include <windows.h>
#include <limits>
#include <string>
#include <string_view>

namespace allfetch::system::windows_platform
{
    inline std::string utf8(std::wstring_view value)
    {
        if (value.empty() || value.size() > (std::numeric_limits<int>::max)())
            return {};
        const int length = static_cast<int>(value.size());
        const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
            value.data(), length, nullptr, 0, nullptr, nullptr);
        if (bytes <= 0)
            return {};
        std::string result(bytes, '\0');
        if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                length, result.data(), bytes, nullptr, nullptr))
            return {};
        return result;
    }

    inline std::string utf8OrUnknown(std::wstring_view value)
    {
        auto text = utf8(value);
        if (text.empty()) return "Unknown";
        return text;
    }

    inline const char *architecture()
    {
        SYSTEM_INFO system{};
        GetNativeSystemInfo(&system);
        switch (system.wProcessorArchitecture)
        {
        case PROCESSOR_ARCHITECTURE_AMD64: return "x86_64";
        case PROCESSOR_ARCHITECTURE_ARM64: return "aarch64";
        case PROCESSOR_ARCHITECTURE_INTEL: return "x86";
        case PROCESSOR_ARCHITECTURE_ARM: return "ARM";
        default: return "Unknown";
        }
    }
}
