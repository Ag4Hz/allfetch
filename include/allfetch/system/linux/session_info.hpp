#pragma once

#include "allfetch/system/system_info.hpp"
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>
#include <nlohmann/json.hpp>

namespace allfetch::system::linux_session
{
    inline std::string env(const char *key)
    {
        const char *value = std::getenv(key);
        return value ? value : "";
    }

    inline std::string query(const std::string &command)
    {
        const auto invocation = std::string("LC_ALL=C timeout -k 0.2s 1s ") + command + " </dev/null 2>/dev/null";
        FILE *pipe = popen(invocation.c_str(), "r");
        if (!pipe) return {};
        std::string output;
        char buffer[4096];
        while (output.size() <= 262144 && fgets(buffer, sizeof(buffer), pipe))
            output += buffer;
        if (pclose(pipe) != 0 || output.size() > 262144) return {};
        return output;
    }

    inline std::string versionAfter(const std::string &command, std::string_view prefix)
    {
        const auto output = query(command);
        const auto start = output.find(prefix);
        if (start == std::string::npos)
            return "Unknown";
        const auto first = start + prefix.size();
        if (first == output.size() || output[first] < '0' || output[first] > '9')
            return "Unknown";
        const auto end = output.find_first_of(" \t\r\n(", first);
        return output.substr(first, end == std::string::npos ? end : end - first);
    }

    inline void paletteEntry(ColorsInfo &colors, const std::string &key, const std::string &rgb)
    {
        if (rgb.size() != 7 || rgb.front() != '#' ||
            rgb.find_first_not_of("0123456789abcdefABCDEF", 1) != std::string::npos) return;
        for (size_t i = 0; i < colors.hexValues.size(); ++i)
            if (key == "color" + std::to_string(i)) colors.hexValues[i] = rgb;
    }

    inline void kitty(SystemInfo &info, const std::filesystem::path &config, bool collectColors)
    {
        info.terminalInfo.name = "kitty";
        if (info.terminalInfo.version == "Unknown")
            info.terminalInfo.version = versionAfter("kitty --version", "kitty ");
        const auto path = config / "kitty/kitty.conf";
        std::ifstream file(path);
        std::string line, key, value;
        while (std::getline(file, line))
        {
            std::istringstream fields(line);
            if (!(fields >> key) || !std::getline(fields >> std::ws, value)) continue;
            if (key == "font_family")
            {
                info.terminalInfo.font = value;
                info.terminalInfo.fontSource = path.string();
            }
            if (collectColors) paletteEntry(info.colorsInfo, key, value);
        }
        // A socket keeps palette replies separate from keyboard input.
        const auto window = env("KITTY_WINDOW_ID");
        if (collectColors && env("KITTY_LISTEN_ON").starts_with("unix:") &&
            !window.empty() && window.find_first_not_of("0123456789") == std::string::npos)
        {
            std::istringstream colors(query(
                R"(kitten @ --to "$KITTY_LISTEN_ON" get-colors --match "id:$KITTY_WINDOW_ID")"));
            while (std::getline(colors, line))
            {
                std::istringstream fields(line);
                if (fields >> key >> value) paletteEntry(info.colorsInfo, key, value);
            }
        }
    }

    inline void collectShellVersion(ShellInfo &shell)
    {
        if (!std::filesystem::path(shell.executable).is_absolute()) return;
        std::string_view prefix;
        if (shell.name == "Zsh") prefix = "zsh ";
        else if (shell.name == "Bash") prefix = "GNU bash, version ";
        else if (shell.name == "Fish") prefix = "fish, version ";
        else return;

        // Quote the detected executable as a single POSIX shell argument.
        std::string command = "'";
        for (const char c : shell.executable)
        {
            if (c == '\'') command += "'\\''";
            else command += c;
        }
        shell.version = versionAfter(command + "' --version", prefix);
    }

    inline std::string_view connectorName(std::string_view name)
    {
        if (name.starts_with("card"))
        {
            const auto end = name.find_first_not_of("0123456789", 4);
            if (end != name.npos && end > 4 && name[end] == '-')
                name.remove_prefix(end + 1);
        }
        return name;
    }

    inline void mergeDisplays(std::vector<DisplayInfo> &displays, const nlohmann::json &monitors)
    {
        if (!monitors.is_array()) return;
        for (const auto &monitor : monitors)
        {
            const auto name = monitor.find("name");
            if (name == monitor.end() || !name->is_string()) continue;
            const auto &connector = name->get_ref<const std::string &>();
            if (connector.empty()) continue;
            auto match = std::ranges::find_if(displays, [&](const DisplayInfo &display)
            {
                return connectorName(display.name) == connector;
            });
            auto &display = match == displays.end() ? displays.emplace_back() : *match;
            display.name = connector;
            display.platformApi = "Hyprland IPC";
            if (connector.starts_with("eDP-")) display.type = "Builtin";
            const auto integer = [&](const char *key, auto &field, int minimum)
            {
                const auto it = monitor.find(key);
                if (it == monitor.end() || !it->is_number_integer()) return;
                using T = std::remove_reference_t<decltype(field)>;
                if (*it >= minimum && *it <= (std::numeric_limits<T>::max)())
                    field = it->get<T>();
            };
            integer("id", display.id, 0);
            integer("width", display.width, 1);
            integer("height", display.height, 1);
            const auto hz = monitor.find("refreshRate");
            if (hz != monitor.end() && hz->is_number())
            {
                const double rate = hz->get<double>();
                if (rate > 0 && std::isfinite(rate)) display.refreshRateHz = rate;
            }
        }
    }

    inline void collect(SystemInfo &info, bool collectColors)
    {
        collectShellVersion(info.shellInfo);
        if (!env("SSH_CONNECTION").empty() || !env("SSH_TTY").empty()) return;
        if (info.terminalInfo.name == "GNOME Terminal" && info.terminalInfo.version == "Unknown")
            info.terminalInfo.version = versionAfter("gnome-terminal --version", "GNOME Terminal ");
        const auto xdg = env("XDG_CONFIG_HOME");
        const std::filesystem::path config = xdg.empty() ? env("HOME") + "/.config" : xdg;
        if (env("TMUX").empty() && env("STY").empty() &&
            (info.terminalInfo.name == "Unknown" || info.terminalInfo.name == "kitty") &&
            (!env("KITTY_WINDOW_ID").empty() || env("TERM") == "xterm-kitty"))
            kitty(info, config, collectColors);
        if (env("HYPRLAND_INSTANCE_SIGNATURE").empty() || env("WAYLAND_DISPLAY").empty()) return;
        info.windowManagerInfo.name = "Hyprland";
        info.windowManagerInfo.protocol = "Wayland";
        const auto version = nlohmann::json::parse(query("hyprctl -j version"), nullptr, false);
        for (const char *key : {"version", "tag"})
            if (version.contains(key) && version[key].is_string() && version[key] != "")
            {
                info.windowManagerInfo.version = version[key].get<std::string>();
                break;
            }
        const auto monitors = nlohmann::json::parse(query("hyprctl -j monitors"), nullptr, false);
        mergeDisplays(info.displayInfo, monitors);
        auto &appearance = info.desktopAppearanceInfo;
        std::ifstream gtk(config / "gtk-3.0/settings.ini");
        std::string line;
        bool active = false;
        while (std::getline(gtk, line))
        {
            if (line.starts_with("[")) active = line == "[Settings]";
            const auto sep = line.find('=');
            if (!active || sep == std::string::npos) continue;
            auto key = line.substr(0, sep), value = line.substr(sep + 1);
            if (key == "gtk-theme-name") appearance.theme = value;
            if (key == "gtk-icon-theme-name") appearance.iconTheme = value;
            if (key == "gtk-font-name") appearance.font = value;
            if (key == "gtk-cursor-theme-name") appearance.cursorTheme = value;
        }
        if (!env("XCURSOR_THEME").empty()) appearance.cursorTheme = env("XCURSOR_THEME");
        int size;
        if (std::istringstream(env("XCURSOR_SIZE")) >> size && size > 0) appearance.cursorSize = size;
    }
}
