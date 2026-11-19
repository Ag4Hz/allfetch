#pragma once

#include "allfetch/system/system_info.hpp"
#include "allfetch/system/terminal/font_settings.hpp"

#include <nlohmann/json.hpp>

namespace allfetch::system::terminal_font_detail
{
    inline void collectVSCodeFont(
        const nlohmann::json &resolvedSettings,
        TerminalInfo &info)
    {
        auto settings = resolvedSettings;
        const bool automatic = settings.empty();
        bool loaded = false;

        if (automatic)
        {
            std::filesystem::path path;
            const auto portable = environmentPath("VSCODE_PORTABLE");
            if (!portable.empty())
            {
                path = portable / "user-data" / "User" / "settings.json";
            }
            else
            {
#if defined(_WIN32)
                auto root = environmentPath("APPDATA");
#else
                auto root = environmentPath("XDG_CONFIG_HOME");
                if (root.empty())
                {
                    const auto home = environmentPath("HOME");
                    if (!home.empty())
                        root = home / ".config";
                }
#endif
                if (!root.empty())
                {
                    const bool insiders = environment("TERM_PROGRAM_VERSION")
                                              .find("insider") != std::string::npos;
                    path = root / (insiders ? "Code - Insiders" : "Code") /
                           "User" / "settings.json";
                }
            }

            if (!path.empty())
            {
                settings = readFontSettings(path);
                loaded = settings.is_object();
                std::error_code error;
                if (!loaded && std::filesystem::exists(path, error))
                    return; // Do not hide an unreadable or invalid configuration.
            }
        }

        std::string font = stringSetting(
            settings,
            "terminal.integrated.fontFamily");

        if (font.empty())
        {
            font = stringSetting(
                settings,
                "editor.fontFamily");
        }

        if (font.empty())
        {
#if defined(_WIN32)
            font = "Consolas, 'Courier New', monospace";
#else
            font = "'Droid Sans Mono', monospace";
#endif
            info.fontSource = "VS Code built-in default (configured)";
        }
        else
        {
            info.fontSource = automatic
                                  ? "VS Code default-profile user settings"
                                  : "VS Code supplied settings";
        }

        info.font = font;
    }
}
