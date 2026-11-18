#pragma once

#include "allfetch/system/system_info.hpp"

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace allfetch::system
{
    namespace title_detail
    {
        inline std::string directory(std::string value)
        {
            if (!value.empty() && value.back() != '/')
                value.push_back('/');

            return value;
        }

        inline void collectAccount(TitleInfo &info)
        {
            const uid_t uid = geteuid();

            info.userId =
                std::to_string(
                    static_cast<unsigned long long>(uid));

            constexpr std::size_t maximumBuffer =
                16 * 1024 * 1024;

            const long suggested =
                sysconf(_SC_GETPW_R_SIZE_MAX);

            std::size_t size =
                suggested > 0 && static_cast<unsigned long long>(suggested) <= maximumBuffer
                    ? static_cast<std::size_t>(suggested)
                    : 16384;

            for (; size <= maximumBuffer; size *= 2)
            {
                std::vector<char> buffer(size);

                passwd account{};
                passwd *result = nullptr;

                const int status = getpwuid_r(
                    uid,
                    &account,
                    buffer.data(),
                    buffer.size(),
                    &result);

                if (status == ERANGE)
                    continue;

                if (status != 0 || result == nullptr)
                    return;

                if (account.pw_name != nullptr)
                    info.userName = account.pw_name;

                if (account.pw_dir != nullptr)
                    info.homeDir = directory(account.pw_dir);

                if (account.pw_shell != nullptr)
                    info.userShell = account.pw_shell;

                if (account.pw_gecos != nullptr)
                {
                    info.fullUserName = account.pw_gecos;

                    // The first GECOS field conventionally holds the name.
                    const auto comma =
                        info.fullUserName.find(',');

                    if (comma != std::string::npos)
                        info.fullUserName.resize(comma);
                }

                return;
            }
        }
    }

    inline TitleInfo getTitleInfo()
    {
        namespace fs = std::filesystem;

        TitleInfo info;
        info.pid = static_cast<long long>(getpid());

        title_detail::collectAccount(info);

        char hostname[256]{};

        if (gethostname(hostname, sizeof(hostname)) == 0)
        {
            hostname[sizeof(hostname) - 1] = '\0';
            info.hostName = hostname;
        }

        std::error_code error;

        const fs::path executable =
            fs::read_symlink("/proc/self/exe", error);

        if (!error)
            info.exePath = executable.string();

        error.clear();

        const fs::path current =
            fs::current_path(error);

        if (!error)
            info.cwd =
                title_detail::directory(current.string());

        return info;
    }
}