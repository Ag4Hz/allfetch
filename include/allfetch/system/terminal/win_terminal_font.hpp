#pragma once

#include "allfetch/system/system_info.hpp"

#include <nlohmann/json.hpp>

#include <cctype>
#include <string>

namespace allfetch::system::terminal_font_detail
{
    inline std::string normalizeGuid(std::string value)
    {
        std::string result;

        for (unsigned char character : value)
        {
            if (character != '{' && character != '}')
            {
                result.push_back(
                    static_cast<char>(std::tolower(character)));
            }
        }

        return result;
    }

    inline std::string stringSetting(
        const nlohmann::json &object,
        const char *key)
    {
        if (!object.is_object())
            return {};

        const auto entry = object.find(key);

        if (entry == object.end() || !entry->is_string())
            return {};

        return entry->get<std::string>();
    }

    inline std::string windowsFontFace(
        const nlohmann::json &object)
    {
        if (!object.is_object())
            return {};

        const auto font = object.find("font");

        if (font != object.end())
        {
            const auto face = stringSetting(*font, "face");

            if (!face.empty())
                return face;
        }

        // Older Windows Terminal settings.
        return stringSetting(object, "fontFace");
    }

    inline void collectWindowsTerminalFont(
        const nlohmann::json &settings,
        const std::string &profileId,
        TerminalInfo &info)
    {
        if (!settings.is_object() || profileId.empty())
            return;

        const auto profiles = settings.find("profiles");

        if (profiles == settings.end() || !profiles->is_object())
            return;

        const auto list = profiles->find("list");

        if (list == profiles->end() || !list->is_array())
            return;

        std::string inheritedFont;

        const auto defaults = profiles->find("defaults");

        if (defaults != profiles->end())
            inheritedFont = windowsFontFace(*defaults);

        for (const auto &profile : *list)
        {
            const auto guid = stringSetting(profile, "guid");

            if (normalizeGuid(guid) != normalizeGuid(profileId))
                continue;

            std::string font = windowsFontFace(profile);

            if (font.empty())
                font = inheritedFont;

            if (!font.empty())
            {
                info.font = font;
                info.fontSource =
                    "Windows Terminal profile settings";
            }

            return;
        }
    }
}