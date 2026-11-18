#pragma once

#include "allfetch/system/system_info.hpp"

#if defined(ALLFETCH_HAS_GIO)

#include <gio/gio.h>

#include <cstdlib>
#include <memory>
#include <string>

namespace allfetch::system::wm_detail
{
    template <typename T>
    using Object = std::unique_ptr<T, decltype(&g_object_unref)>;

    using Variant =
        std::unique_ptr<GVariant, decltype(&g_variant_unref)>;

    using Schema =
        std::unique_ptr<GSettingsSchema,
                        decltype(&g_settings_schema_unref)>;

    using SchemaSource =
        std::unique_ptr<GSettingsSchemaSource,
                        decltype(&g_settings_schema_source_unref)>;

    inline std::string configuredShellTheme(
        const std::string &extensionPath)
    {
        auto *defaultSource =
            g_settings_schema_source_get_default();

        SchemaSource privateSource(
            nullptr, &g_settings_schema_source_unref);

        // User-installed extensions can contain their own schema.
        if (!extensionPath.empty())
        {
            const std::string directory =
                extensionPath + "/schemas";

            privateSource.reset(
                g_settings_schema_source_new_from_directory(
                    directory.c_str(),
                    defaultSource,
                    FALSE,
                    nullptr));
        }

        auto *source = privateSource
                           ? privateSource.get()
                           : defaultSource;

        if (source == nullptr)
            return {};

        Schema schema(
            g_settings_schema_source_lookup(
                source,
                "org.gnome.shell.extensions.user-theme",
                TRUE),
            &g_settings_schema_unref);

        if (!schema || !g_settings_schema_has_key(schema.get(), "name"))
            return {};

        Object<GSettings> settings(
            g_settings_new_full(
                schema.get(), nullptr, nullptr),
            &g_object_unref);

        if (!settings)
            return {};

        std::unique_ptr<gchar, decltype(&g_free)> name(
            g_settings_get_string(settings.get(), "name"),
            &g_free);

        if (!name)
            return {};

        return *name.get() != '\0'
                   ? std::string(name.get())
                   : std::string("Default");
    }
}

#endif

namespace allfetch::system
{
    inline WindowManagerInfo getWindowManagerInfo()
    {
        WindowManagerInfo info;

#if defined(ALLFETCH_HAS_GIO)
        // Avoid starting a session bus on a machine without a desktop.
        const char *address =
            std::getenv("DBUS_SESSION_BUS_ADDRESS");

        if (address == nullptr || address[0] == '\0')
            return info;

        wm_detail::Object<GDBusConnection> bus(
            g_bus_get_sync(
                G_BUS_TYPE_SESSION, nullptr, nullptr),
            &g_object_unref);

        if (!bus)
            return info;

        wm_detail::Variant owner(
            g_dbus_connection_call_sync(
                bus.get(),
                "org.freedesktop.DBus",
                "/org/freedesktop/DBus",
                "org.freedesktop.DBus",
                "NameHasOwner",
                g_variant_new("(s)", "org.gnome.Shell"),
                G_VARIANT_TYPE("(b)"),
                G_DBUS_CALL_FLAGS_NO_AUTO_START,
                1500,
                nullptr,
                nullptr),
            &g_variant_unref);

        if (!owner)
            return info;

        gboolean running = FALSE;
        g_variant_get(owner.get(), "(b)", &running);

        if (!running)
            return info;

        info.name = "Mutter";

        wm_detail::Variant reply(
            g_dbus_connection_call_sync(
                bus.get(),
                "org.gnome.Shell",
                "/org/gnome/Shell",
                "org.gnome.Shell.Extensions",
                "GetExtensionInfo",
                g_variant_new(
                    "(s)",
                    "user-theme@gnome-shell-extensions.gcampax.github.com"),
                G_VARIANT_TYPE("(a{sv})"),
                G_DBUS_CALL_FLAGS_NO_AUTO_START,
                1500,
                nullptr,
                nullptr),
            &g_variant_unref);

        if (!reply)
            return info;

        wm_detail::Variant details(
            g_variant_get_child_value(reply.get(), 0),
            &g_variant_unref);

        // An empty dictionary means the extension is not installed.
        if (g_variant_n_children(details.get()) == 0)
        {
            info.theme = "Default";
            return info;
        }

        gdouble state = 0;

        if (!g_variant_lookup(
                details.get(), "state", "d", &state))
            return info;

        // Inactive, initialized, or uninstalled.
        if (state == 2 || state == 6 || state == 99)
        {
            info.theme = "Default";
            return info;
        }

        // Error or transition states do not establish a current theme.
        if (state != 1)
            return info;

        const gchar *extensionPath = nullptr;

        g_variant_lookup(
            details.get(), "path", "&s", &extensionPath);

        const std::string theme =
            wm_detail::configuredShellTheme(
                extensionPath != nullptr ? extensionPath : "");

        if (!theme.empty())
            info.theme = theme;
#endif

        return info;
    }
}