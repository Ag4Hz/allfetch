#pragma once

#include "allfetch/system/system_info.hpp"
#include "allfetch/system/terminal/win_terminal_font.hpp"

#include <nlohmann/json.hpp>

namespace allfetch::system::terminal_font_detail
{
    inline void collectVSCodeFont(
        const nlohmann::json &resolvedSettings,
        TerminalInfo &info)
    {
        std::string font = stringSetting(
            resolvedSettings,
            "terminal.integrated.fontFamily");

        if (font.empty())
        {
            font = stringSetting(
                resolvedSettings,
                "editor.fontFamily");
        }

        if (!font.empty())
        {
            info.font = font;
            info.fontSource = "VS Code resolved settings";
        }
    }
}