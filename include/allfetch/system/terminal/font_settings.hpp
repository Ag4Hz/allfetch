#pragma once

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace allfetch::system::terminal_font_detail
{
    inline std::string environment(const char *name)
    {
        const char *value = std::getenv(name);
        return value != nullptr ? value : "";
    }

    inline std::filesystem::path environmentPath(const char *name)
    {
#if defined(_WIN32)
        const std::string narrowName(name);
        const std::wstring wideName(narrowName.begin(), narrowName.end());
        const wchar_t *value = _wgetenv(wideName.c_str());
        return value != nullptr ? std::filesystem::path(value)
                                : std::filesystem::path{};
#else
        return environment(name);
#endif
    }

    inline std::string stringSetting(
        const nlohmann::json &object, const char *key)
    {
        if (!object.is_object())
            return {};

        const auto entry = object.find(key);
        return entry != object.end() && entry->is_string()
                   ? entry->get<std::string>()
                   : std::string{};
    }

    inline nlohmann::json readFontSettings(const std::filesystem::path &path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            return nullptr;

        std::string text((std::istreambuf_iterator<char>(file)), {});

        // JSONC comments become whitespace, preserving strings and tokens.
        for (std::size_t index = 0; index < text.size(); ++index)
        {
            if (text[index] == '"')
            {
                while (++index < text.size())
                {
                    if (text[index] == '\\')
                        ++index;
                    else if (text[index] == '"')
                        break;
                }
            }
            else if (text[index] == '/' && index + 1 < text.size())
            {
                std::size_t end = index;
                if (text[index + 1] == '/')
                {
                    end = text.find('\n', index + 2);
                    if (end == std::string::npos)
                        end = text.size();
                }
                else if (text[index + 1] == '*')
                {
                    end = text.find("*/", index + 2);
                    if (end == std::string::npos)
                        return nullptr;
                    end += 2;
                }

                if (end > index)
                {
                    std::fill(text.begin() + index, text.begin() + end, ' ');
                    index = end - 1;
                }
            }
        }

        // Normalize trailing commas before strict JSON parsing.
        for (std::size_t index = 0; index < text.size(); ++index)
        {
            if (text[index] == '"')
            {
                while (++index < text.size())
                {
                    if (text[index] == '\\')
                        ++index;
                    else if (text[index] == '"')
                        break;
                }
            }
            else if (text[index] == ',')
            {
                const auto next = text.find_first_not_of(" \t\r\n", index + 1);
                if (next != std::string::npos &&
                    (text[next] == '}' || text[next] == ']'))
                    text[index] = ' ';
            }
        }

        auto settings = nlohmann::json::parse(text, nullptr, false);
        return settings.is_object() ? settings : nlohmann::json(nullptr);
    }
}
