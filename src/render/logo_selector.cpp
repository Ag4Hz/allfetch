#include "allfetch/render/logo_selector.hpp"

#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace allfetch::render
{
    namespace
    {
        constexpr unsigned char asciiLower(unsigned char c)
        {
            return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
        }

        // Sort by length first so comparisons between different lengths skip the bytes.
        int compareAliases(std::string_view left, std::string_view right)
        {
            if (left.size() != right.size())
                return left.size() < right.size() ? -1 : 1;

            for (std::size_t i = 0; i < left.size(); ++i)
            {
                const auto a = asciiLower(left[i]);
                const auto b = asciiLower(right[i]);
                if (a != b)
                    return a < b ? -1 : 1;
            }
            return 0;
        }

        bool validLogoPath(std::string_view path)
        {
            // Generated paths use one letter (or _) and a plain .txt filename.
            if (path.size() <= 6 || path[1] != '/' || !path.ends_with(".txt") ||
                !((path[0] >= 'a' && path[0] <= 'z') || path[0] == '_'))
                return false;

            return std::ranges::all_of(path.substr(2, path.size() - 6), [](unsigned char c)
            {
                const auto lower = asciiLower(c);
                return (lower >= 'a' && lower <= 'z') ||
                       (c >= '0' && c <= '9') || c == '_' || c == '-';
            });
        }

        bool validColor(std::string_view color)
        {
            if (color.empty() || color.size() > 32)
                return false;

            bool needsDigit = true;
            for (const char c : color)
            {
                if (c >= '0' && c <= '9')
                    needsDigit = false;
                else if (c == ';' && !needsDigit)
                    needsDigit = true;
                else
                    return false;
            }
            return !needsDigit;
        }
    }

    LogoIndex LogoIndex::load(const std::filesystem::path &indexFile)
    {
        std::ifstream input(indexFile, std::ios::binary);
        if (!input)
            throw std::runtime_error("Cannot open logo index: " + indexFile.string());

        LogoIndex index;
        try
        {
            const auto document = nlohmann::json::parse(input);
            if (!document.is_array())
                throw std::runtime_error("Logo index must contain an array");

            index.logos.reserve(document.size());
            index.aliases.reserve(document.size());
            for (const auto &entry : document)
            {
                if (entry.at("small").get<bool>() || entry.at("alternate").get<bool>())
                    continue;

                const auto &path = entry.at("path").get_ref<const std::string &>();
                if (!validLogoPath(path))
                    throw std::runtime_error("Invalid relative logo path");
                LogoEntry logo{path, {}};
                if (const auto colors = entry.find("colors"); colors != entry.end())
                {
                    if (!colors->is_array() || colors->size() > 9)
                        throw std::runtime_error("Invalid logo color slots");
                    for (const auto &slot : *colors)
                    {
                        const auto &color = slot.get_ref<const std::string &>();
                        if (!validColor(color))
                            throw std::runtime_error("Invalid logo color");
                        logo.colors.push_back(color);
                    }
                }

                const auto &names = entry.at("aliases");
                if (!names.is_array() || names.empty())
                    throw std::runtime_error("Logo aliases must be a nonempty array");

                for (const auto &name : names)
                {
                    const auto &alias = name.get_ref<const std::string &>();
                    if (alias.empty() || alias.find('\0') != std::string::npos)
                        throw std::runtime_error("Invalid logo alias");
                    index.aliases.push_back({alias, index.logos.size()});
                }
                index.logos.push_back(std::move(logo));
            }
        }
        catch (const nlohmann::json::exception &error)
        {
            throw std::runtime_error(std::string("Invalid logo index: ") + error.what());
        }

        std::sort(index.aliases.begin(), index.aliases.end(), [](const Alias &a, const Alias &b)
        {
            return compareAliases(a.name, b.name) < 0;
        });
        for (std::size_t i = 1; i < index.aliases.size(); ++i)
        {
            const auto &previous = index.aliases[i - 1];
            const auto &current = index.aliases[i];
            if (compareAliases(previous.name, current.name) == 0 &&
                (index.logos[previous.logo].path != index.logos[current.logo].path ||
                 index.logos[previous.logo].colors != index.logos[current.logo].colors))
                throw std::runtime_error("Ambiguous logo alias: " + current.name);
        }
        return index;
    }

    const LogoEntry *LogoIndex::find(std::string_view name) const
    {
        if (name.empty())
            return nullptr;

        const auto match = std::lower_bound(aliases.begin(), aliases.end(), name,
            [](const Alias &alias, std::string_view key)
            {
                return compareAliases(alias.name, key) < 0;
            });
        if (match != aliases.end() && compareAliases(match->name, name) == 0)
            return &logos[match->logo];
        return nullptr;
    }

    std::optional<std::string_view> LogoIndex::selectLogo(const system::OSInfo &os) const &
    {
        if (const auto *logo = selectEntry(os))
            return std::string_view(logo->path);
        return std::nullopt;
    }

    const LogoEntry *LogoIndex::selectEntry(const system::OSInfo &os) const &
    {
        const auto findKnown = [this](std::string_view name)
        {
            return compareAliases(name, "unknown") == 0 ? nullptr : find(name);
        };

        if (auto logo = findKnown(os.id))
            return logo;
        if (auto logo = findKnown(os.name))
            return logo;

        // Windows reports edition suffixes. Try the longest registered product name first.
        std::string_view product = os.name;
        if (compareAliases(product.substr(0, 8), "Windows ") == 0)
        {
            for (auto space = product.rfind(' '); space != std::string_view::npos; space = product.rfind(' '))
            {
                product = product.substr(0, space);
                if (auto logo = find(product))
                    return logo;
            }
        }

        for (const auto &family : os.idLike)
            if (auto logo = findKnown(family))
                return logo;
        if (auto logo = findKnown(os.kernelName))
            return logo;
        return find("unknown");
    }
}
