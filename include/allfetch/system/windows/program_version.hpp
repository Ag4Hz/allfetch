#pragma once

#include <windows.h>
#include <winver.h>

#include <filesystem>
#include <string>
#include <vector>

namespace allfetch::system::windows_version_detail
{
    inline std::string formatVersion(unsigned major, unsigned minor, unsigned build, unsigned revision)
    {
        return std::to_string(major) + '.' + std::to_string(minor) + '.' +
            std::to_string(build) + '.' + std::to_string(revision);
    }

    inline std::string fileVersion(const std::filesystem::path &path)
    {
        if (!path.is_absolute())
            return {};
        const DWORD bytes = GetFileVersionInfoSizeW(path.c_str(), nullptr);
        if (bytes == 0)
            return {};

        std::vector<BYTE> buffer(bytes);
        if (!GetFileVersionInfoW(path.c_str(), 0, bytes, buffer.data()))
            return {};

        void *data = nullptr;
        UINT size = 0;
        if (!VerQueryValueW(buffer.data(), L"\\", &data, &size) || size < sizeof(VS_FIXEDFILEINFO))
            return {};
        const auto &version = *static_cast<const VS_FIXEDFILEINFO *>(data);
        if (version.dwSignature != 0xfeef04bd || (!version.dwFileVersionMS && !version.dwFileVersionLS))
            return {};
        return formatVersion(HIWORD(version.dwFileVersionMS), LOWORD(version.dwFileVersionMS),
            HIWORD(version.dwFileVersionLS), LOWORD(version.dwFileVersionLS));
    }
}
