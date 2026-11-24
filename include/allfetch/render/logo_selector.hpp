#pragma once

#include "allfetch/system/system_info.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace allfetch::render
{
    class LogoIndex
    {
    public:
        // Throws std::runtime_error if the index cannot be read or is invalid.
        static LogoIndex load(const std::filesystem::path &indexFile);

        // Returns a relative art path borrowed from this index.
        std::optional<std::string_view> selectLogo(const system::OSInfo &os) const &;
        std::optional<std::string_view> selectLogo(const system::OSInfo &) const && = delete;

    private:
        struct Alias
        {
            std::string name;
            std::size_t logo;
        };

        std::optional<std::string_view> find(std::string_view name) const;

        std::vector<Alias> aliases;
        std::vector<std::string> paths;
    };
}
