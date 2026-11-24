#pragma once

#include "allfetch/system/system_info.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string_view>

namespace allfetch::system
{
    namespace packages_detail
    {
        namespace fs = std::filesystem;

        inline std::optional<std::size_t> countDpkg(const fs::path &path)
        {
            std::ifstream status(path);
            if (!status)
                return std::nullopt;

            std::size_t count = 0;
            for (std::string line; std::getline(status, line);)
            {
                if (!line.starts_with("Status:"))
                    continue;
                std::string_view field(line);
                const auto end = field.find_last_not_of(" \t\r");
                field = field.substr(0, end + 1);
                const auto separator = field.find_last_of(" \t");
                if (separator != std::string_view::npos && field.substr(separator + 1) == "installed")
                    ++count;
            }
            return status.eof() && !status.bad() ? std::optional{count} : std::nullopt;
        }

        inline std::optional<std::size_t> countPacman(const fs::path &path)
        {
            std::error_code error;
            fs::directory_iterator entry(path, error), end;
            std::size_t count = 0;
            for (; !error && entry != end; entry.increment(error))
            {
                if (!entry->is_directory(error))
                    continue;
                if (fs::is_regular_file(entry->path() / "desc", error))
                    ++count;
                else if (error == std::errc::no_such_file_or_directory)
                    error.clear();
            }
            return error ? std::nullopt : std::optional{count};
        }

        inline bool hasRpmDatabase()
        {
            for (const auto *directory : {"/usr/lib/sysimage/rpm", "/var/lib/rpm"})
                for (const auto *name : {"rpmdb.sqlite", "Packages.db", "Packages"})
                {
                    std::error_code error;
                    if (fs::is_regular_file(fs::path(directory) / name, error))
                        return true;
                }
            return false;
        }

        // The callers supply fixed commands with a timeout. Count output only on success.
        inline std::optional<std::size_t> countCommand(const char *command, std::string_view skip = {})
        {
            FILE *output = popen(command, "r");
            if (!output)
                return std::nullopt;

            char *line = nullptr;
            std::size_t capacity = 0;
            std::size_t count = 0;
            for (auto length = ::getline(&line, &capacity, output); length >= 0;
                 length = ::getline(&line, &capacity, output))
            {
                std::string_view name(line, static_cast<std::size_t>(length));
                if (name.ends_with('\n'))
                    name.remove_suffix(1);
                if (!name.empty() && name != skip)
                    ++count;
            }
            std::free(line);
            const bool complete = std::feof(output) && !std::ferror(output);
            const int status = pclose(output);
            return complete && status == 0 ? std::optional{count} : std::nullopt;
        }
    }

    inline std::vector<PackageCount> getPackagesInfo()
    {
        using namespace packages_detail;
        std::vector<PackageCount> packages;
        const auto add = [&](const char *manager, std::optional<std::size_t> count)
        {
            if (count)
                packages.push_back({manager, *count});
        };
        add("pacman", countPacman("/var/lib/pacman/local"));
        add("dpkg", countDpkg("/var/lib/dpkg/status"));
        if (hasRpmDatabase())
            add("rpm", countCommand(
                R"(/usr/bin/timeout -k 1 2 /usr/bin/rpm -qa --queryformat '%{NAME}\n' </dev/null 2>/dev/null)",
                "gpg-pubkey"));

        std::error_code error;
        if (fs::is_regular_file("/sbin/apk", error))
            add("apk", countCommand("/usr/bin/timeout -k 1 2 /sbin/apk info </dev/null 2>/dev/null"));
        else if (fs::is_regular_file("/usr/bin/apk", error))
            add("apk", countCommand("/usr/bin/timeout -k 1 2 /usr/bin/apk info </dev/null 2>/dev/null"));
        return packages;
    }
}
