#pragma once

#include <windows.h>
#include <tlhelp32.h>
#include <memory>
#include <string>
#include <unordered_map>

namespace allfetch::system::windows_platform
{
    struct ProcessEntry
    {
        DWORD parent;
        std::wstring executable;
    };

    using ProcessList = std::unordered_map<DWORD, ProcessEntry>;

    inline ProcessList readProcesses()
    {
        ProcessList processes;
        HANDLE raw = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (raw == INVALID_HANDLE_VALUE)
            return processes;
        std::unique_ptr<void, decltype(&CloseHandle)> snapshot(raw, &CloseHandle);
        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        for (BOOL ok = Process32FirstW(raw, &entry); ok; ok = Process32NextW(raw, &entry))
            processes.emplace(entry.th32ProcessID, ProcessEntry{entry.th32ParentProcessID, entry.szExeFile});
        return processes;
    }
}
