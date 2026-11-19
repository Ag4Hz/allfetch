#pragma once

#include "allfetch/system/system_info.hpp"

#if defined(ALLFETCH_HAS_GIO)
#include <gio/gio.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <memory>
#include <string>

namespace allfetch::system::appearance_detail
{
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
}
#endif

namespace allfetch::system
{
    inline DesktopAppearanceInfo getDesktopAppearanceInfo()
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
