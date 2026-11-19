#pragma once

#include "allfetch/system/system_info.hpp"

#if defined(ALLFETCH_HAS_GIO)

#include <gio/gio.h>

#include <memory>
#include <string>

namespace allfetch::system::terminal_font_detail
{
    using Settings =
        std::unique_ptr<GSettings, decltype(&g_object_unref)>;

    inline GSettings *openSettings(
        const char *schemaName,
        const char *path = nullptr)
    {
        auto *source = g_settings_schema_source_get_default();

        if (source == nullptr)
            return nullptr;

        auto *schema = g_settings_schema_source_lookup(
            source, schemaName, TRUE);

        if (schema == nullptr)
            return nullptr;

        auto *settings =
            g_settings_new_full(schema, nullptr, path);

        g_settings_schema_unref(schema);
        return settings;
    }

    inline std::string settingsString(
        GSettings *settings,
        const char *key)
    {
        std::unique_ptr<gchar, decltype(&g_free)> value(
            g_settings_get_string(settings, key),
            &g_free);

        return value ? std::string(value.get()) : std::string{};
    }

    inline void collectGnomeTerminalFont(
        const std::string &profileId,
        TerminalInfo &info)
    {
        if (!g_uuid_string_is_valid(profileId.c_str()))
            return;

        const std::string path =
            "/org/gnome/terminal/legacy/profiles:/:" + profileId + "/";

        Settings profile(
            openSettings(
                "org.gnome.Terminal.Legacy.Profile",
                path.c_str()),
            &g_object_unref);

        if (!profile)
            return;

        std::string font;

        if (g_settings_get_boolean(
                profile.get(), "use-system-font"))
        {
            Settings desktop(
                openSettings("org.gnome.desktop.interface"),
                &g_object_unref);

            if (!desktop)
                return;

            font = settingsString(
                desktop.get(), "monospace-font-name");
        }
        else
        {
            font = settingsString(profile.get(), "font");
        }

        if (!font.empty())
        {
            info.font = font;
            info.fontSource =
                "GNOME Terminal profile settings";
        }
    }
}

#endif