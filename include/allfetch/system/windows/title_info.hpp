#pragma once

#include "allfetch/system/system_info.hpp"

#include <windows.h>
#include <sddl.h>
#include <userenv.h>
#include <lm.h>

#include <algorithm>
#include <cwchar>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace allfetch::system
{
    namespace title_detail
    {
        struct HandleCloser
        {
            void operator()(void *handle) const
            {
                CloseHandle(handle);
            }
        };

        using LocalMemory =
            std::unique_ptr<void, decltype(&LocalFree)>;

        inline std::string utf8(std::wstring_view value)
        {
            if (value.empty())
                return {};

            const int length = static_cast<int>(value.size());

            const int required = WideCharToMultiByte(
                CP_UTF8, WC_ERR_INVALID_CHARS,
                value.data(), length,
                nullptr, 0, nullptr, nullptr);

            if (required <= 0)
                return {};

            std::string result(required, '\0');

            if (!WideCharToMultiByte(
                    CP_UTF8, WC_ERR_INVALID_CHARS,
                    value.data(), length,
                    result.data(), required,
                    nullptr, nullptr))
                return {};

            return result;
        }

        inline std::string path(
            std::wstring_view value,
            bool directory = false)
        {
            std::string result = utf8(value);

            std::replace(
                result.begin(), result.end(), '\\', '/');

            if (directory && !result.empty() && result.back() != '/')
                result.push_back('/');

            return result;
        }

        inline void collectAccount(
            HANDLE token,
            TitleInfo &info)
        {
            DWORD required = 0;

            GetTokenInformation(
                token, TokenUser,
                nullptr, 0, &required);

            if (required == 0)
                return;

            LocalMemory storage(
                LocalAlloc(LPTR, required), &LocalFree);

            if (!storage)
                return;

            if (!GetTokenInformation(
                    token, TokenUser,
                    storage.get(), required, &required))
                return;

            const auto *user =
                static_cast<const TOKEN_USER *>(storage.get());

            LPWSTR sidText = nullptr;

            if (ConvertSidToStringSidW(user->User.Sid, &sidText))
            {
                LocalMemory sidStorage(sidText, &LocalFree);
                info.userId = utf8(sidText);
            }

            DWORD nameSize = 0;
            DWORD domainSize = 0;
            SID_NAME_USE accountType{};

            LookupAccountSidW(
                nullptr, user->User.Sid,
                nullptr, &nameSize,
                nullptr, &domainSize,
                &accountType);

            if (nameSize == 0)
                return;

            std::vector<wchar_t> name(nameSize);
            std::vector<wchar_t> domain(
                domainSize > 0 ? domainSize : 1);

            domainSize = static_cast<DWORD>(domain.size());

            if (!LookupAccountSidW(
                    nullptr, user->User.Sid,
                    name.data(), &nameSize,
                    domain.data(), &domainSize,
                    &accountType))
                return;

            info.userName = utf8(name.data());

            // Query the local account database only for local accounts.
            wchar_t computer[MAX_COMPUTERNAME_LENGTH + 1]{};
            DWORD computerSize =
                MAX_COMPUTERNAME_LENGTH + 1;

            if (!GetComputerNameW(computer, &computerSize) || _wcsicmp(domain.data(), computer) != 0)
                return;

            LPBYTE rawAccount = nullptr;

            const NET_API_STATUS status = NetUserGetInfo(
                nullptr, name.data(), 10, &rawAccount);

            std::unique_ptr<void, decltype(&NetApiBufferFree)>
                account(rawAccount, &NetApiBufferFree);

            if (status != NERR_Success || !account)
                return;

            const auto *details =
                static_cast<const USER_INFO_10 *>(account.get());

            if (details->usri10_full_name != nullptr)
                info.fullUserName =
                    utf8(details->usri10_full_name);
        }

        inline void collectProfile(
            HANDLE token,
            TitleInfo &info)
        {
            DWORD required = 0;

            GetUserProfileDirectoryW(
                token, nullptr, &required);

            if (required == 0)
                return;

            std::vector<wchar_t> buffer(required);

            if (GetUserProfileDirectoryW(
                    token, buffer.data(), &required))
            {
                info.homeDir = path(buffer.data(), true);
            }
        }
    }

    inline TitleInfo getTitleInfo()
    {
        TitleInfo info;
        info.pid = GetCurrentProcessId();

        wchar_t hostname[256]{};
        DWORD hostnameSize = 256;

        if (GetComputerNameExW(
                ComputerNameDnsHostname,
                hostname, &hostnameSize))
        {
            info.hostName = title_detail::utf8(
                std::wstring_view(hostname, hostnameSize));
        }

        // Query the executable path without assuming MAX_PATH.
        for (DWORD size = 256; size <= 65536; size *= 2)
        {
            std::vector<wchar_t> buffer(size);
            DWORD length = size;

            if (QueryFullProcessImageNameW(
                    GetCurrentProcess(), 0,
                    buffer.data(), &length))
            {
                info.exePath = title_detail::path(
                    std::wstring_view(buffer.data(), length));

                break;
            }

            if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
                break;
        }

        const DWORD cwdSize =
            GetCurrentDirectoryW(0, nullptr);

        if (cwdSize > 0)
        {
            std::vector<wchar_t> buffer(cwdSize);

            const DWORD length =
                GetCurrentDirectoryW(cwdSize, buffer.data());

            if (length > 0 && length < cwdSize)
            {
                info.cwd = title_detail::path(
                    std::wstring_view(buffer.data(), length),
                    true);
            }
        }

        HANDLE rawToken = nullptr;

        if (OpenProcessToken(
                GetCurrentProcess(), TOKEN_QUERY, &rawToken))
        {
            std::unique_ptr<void, title_detail::HandleCloser>
                token(rawToken);

            title_detail::collectAccount(token.get(), info);
            title_detail::collectProfile(token.get(), info);
        }

        // Windows has no POSIX-style account login-shell field.
        // Leave userShell empty.
        return info;
    }
}