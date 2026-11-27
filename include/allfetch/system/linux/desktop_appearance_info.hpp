#pragma once

#include "allfetch/system/system_info.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#if defined(ALLFETCH_HAS_GIO)
#include <gio/gio.h>

#include <algorithm>
#include <cctype>
#include <memory>
#endif

namespace allfetch::system::appearance_detail
{
    inline std::string_view trimGtkSetting(std::string_view text)
    {
        constexpr std::string_view whitespace = " \t\r";
        const auto first = text.find_first_not_of(whitespace);
        if (first == text.npos)
            return {};
        return text.substr(first, text.find_last_not_of(whitespace) - first + 1);
    }

    inline void readGtkSettings(const std::filesystem::path &path, DesktopAppearanceInfo &info)
    {
        std::ifstream file(path);
        bool settings = false;
        for (std::string line; std::getline(file, line);)
        {
            const auto text = trimGtkSetting(line);
            if (text.starts_with('['))
                settings = text == "[Settings]";
            if (!settings || text.starts_with('#'))
                continue;
            const auto separator = text.find('=');
            if (separator == text.npos)
                continue;
            const auto key = trimGtkSetting(text.substr(0, separator));
            const auto value = trimGtkSetting(text.substr(separator + 1));
            if (value.empty())
                continue;

            if (key == "gtk-theme-name" && info.theme == "Unknown")
                info.theme = value;
            else if (key == "gtk-icon-theme-name" && info.iconTheme == "Unknown")
                info.iconTheme = value;
            else if (key == "gtk-font-name" && info.font == "Unknown")
                info.font = value;
            else if (key == "gtk-cursor-theme-name" && info.cursorTheme == "Unknown")
                info.cursorTheme = value;
        }
    }

    inline void readGtkFallback(DesktopAppearanceInfo &info)
    {
        if (const char *theme = std::getenv("GTK_THEME"); theme && *theme)
            info.theme = theme;

        if (info.theme != "Unknown" && info.iconTheme != "Unknown" &&
            info.font != "Unknown" && info.cursorTheme != "Unknown")
            return;

        std::filesystem::path userConfig;
        if (const char *xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg)
            userConfig = xdg;
        if (!userConfig.is_absolute())
            if (const char *home = std::getenv("HOME"); home && *home)
                userConfig = std::filesystem::path(home) / ".config";

        const auto readConfigDirectory = [&](const std::filesystem::path &dir)
        {
            if (dir.is_absolute())
                for (const char *gtk : {"gtk-4.0", "gtk-3.0"})
                    readGtkSettings(dir / gtk / "settings.ini", info);
        };
        readConfigDirectory(userConfig);

        const char *configuredDirs = std::getenv("XDG_CONFIG_DIRS");
        std::string_view dirs = configuredDirs && *configuredDirs ? configuredDirs : "/etc/xdg";
        while (!dirs.empty())
        {
            const auto separator = dirs.find(':');
            readConfigDirectory(std::filesystem::path(dirs.substr(0, separator)));
            if (separator == dirs.npos)
                break;
            dirs.remove_prefix(separator + 1);
        }
        readConfigDirectory("/etc");
    }

#if defined(ALLFETCH_HAS_GIO)
    inline bool isGnomeSession()
    {
        const char *desktop = std::getenv("XDG_CURRENT_DESKTOP");
        std::string name = desktop != nullptr ? desktop : "";
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char character)
                       { return static_cast<char>(std::tolower(character)); });

        // XDG_CURRENT_DESKTOP is a colon-separated list, e.g. ubuntu:GNOME.
        if (!name.empty())
            return (":" + name + ":").find(":gnome:") != std::string::npos;

        const char *session = std::getenv("XDG_SESSION_DESKTOP");
        name = session != nullptr ? session : "";
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char character)
                       { return static_cast<char>(std::tolower(character)); });
        return name == "gnome" || name.starts_with("gnome-");
    }
#endif

    inline DesktopAppearanceInfo readDesktopSettings()
    {
        DesktopAppearanceInfo info;

#if defined(ALLFETCH_HAS_GIO)
        // Do not mistake installed GNOME schema defaults for another desktop.
        if (!appearance_detail::isGnomeSession())
            return info;

        auto *source = g_settings_schema_source_get_default();
        if (source == nullptr)
            return info;

        std::unique_ptr<GSettingsSchema, decltype(&g_settings_schema_unref)> schema(
            g_settings_schema_source_lookup(source, "org.gnome.desktop.interface", TRUE),
            &g_settings_schema_unref);
        if (!schema)
            return info;

        std::unique_ptr<GSettings, decltype(&g_object_unref)> settings(
            g_settings_new_full(schema.get(), nullptr, nullptr), &g_object_unref);

        const auto readString = [&](const char *key, std::string &destination)
        {
            if (!g_settings_schema_has_key(schema.get(), key))
                return;
            std::unique_ptr<GVariant, decltype(&g_variant_unref)> value(
                g_settings_get_value(settings.get(), key), &g_variant_unref);
            if (g_variant_is_of_type(value.get(), G_VARIANT_TYPE_STRING))
            {
                const char *text = g_variant_get_string(value.get(), nullptr);
                if (text[0] != '\0')
                    destination = text;
            }
        };

        readString("gtk-theme", info.theme);
        readString("icon-theme", info.iconTheme);
        readString("cursor-theme", info.cursorTheme);
        readString("font-name", info.font);

        std::string scheme;
        readString("color-scheme", scheme);
        if (scheme == "prefer-dark")
            info.colorScheme = "Dark";
        else if (scheme == "prefer-light")
            info.colorScheme = "Light";
        else if (scheme == "default")
            info.colorScheme = "Default";

        if (g_settings_schema_has_key(schema.get(), "cursor-size"))
        {
            std::unique_ptr<GVariant, decltype(&g_variant_unref)> value(
                g_settings_get_value(settings.get(), "cursor-size"), &g_variant_unref);
            if (g_variant_is_of_type(value.get(), G_VARIANT_TYPE_INT32))
            {
                const int size = g_variant_get_int32(value.get());
                if (size > 0)
                    info.cursorSize = size;
            }
        }
#endif

        return info;
    }
}

namespace allfetch::system
{
    inline DesktopAppearanceInfo getDesktopAppearanceInfo()
    {
        auto info = appearance_detail::readDesktopSettings();
        appearance_detail::readGtkFallback(info);
        return info;
    }
}
