#pragma once

#include "allfetch/system/system_info.hpp"
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#include <nlohmann/json.hpp>

namespace allfetch::system::linux_session
{
    inline std::string env(const char *key)
    {
        const char *value = std::getenv(key);
        return value ? value : "";
    }

    // Only fixed commands are passed here. Capture their output for JSON mode.
    inline std::string query(const char *command)
    {
        const auto invocation = std::string("timeout -k 0.2s 1s ") + command + " </dev/null 2>/dev/null";
        FILE *pipe = popen(invocation.c_str(), "r");
        if (!pipe) return {};
        std::string output;
        char buffer[4096];
        while (output.size() <= 262144 && fgets(buffer, sizeof(buffer), pipe))
            output += buffer;
        if (pclose(pipe) != 0 || output.size() > 262144) return {};
        return output;
    }

    inline std::string versionOf(const char *command, const char *expected)
    {
        std::istringstream fields(query(command));
        std::string name, version;
        if (fields >> name >> version && name == expected) return version;
        return "Unknown";
    }

    inline void paletteEntry(ColorsInfo &colors, const std::string &key, const std::string &rgb)
    {
        if (rgb.size() != 7 || rgb.front() != '#' ||
            rgb.find_first_not_of("0123456789abcdefABCDEF", 1) != std::string::npos) return;
        for (size_t i = 0; i < colors.hexValues.size(); ++i)
            if (key == "color" + std::to_string(i)) colors.hexValues[i] = rgb;
    }

    // Kitty answers OSC 4 on the controlling terminal, even when stdout is piped.
    inline void queryPalette(ColorsInfo &colors)
    {
        const int fd = open("/dev/tty", O_RDWR | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) return;
        struct Terminal
        {
            int fd;
            termios original{};
            bool changed = false;
            ~Terminal()
            {
                if (changed) tcsetattr(fd, TCSANOW, &original);
                close(fd);
            }
        } terminal{fd};
        if (tcgetpgrp(fd) != getpgrp() || tcgetattr(fd, &terminal.original) != 0) return;
        auto mode = terminal.original;
        mode.c_lflag &= ~(ICANON | ECHO);
        if (tcsetattr(fd, TCSANOW, &mode) != 0) return;
        terminal.changed = true;
        std::string requests;
        for (int i = 0; i < 16; ++i)
            requests += "\033]4;" + std::to_string(i) + ";?\033\\";
        if (write(fd, requests.data(), requests.size()) != static_cast<ssize_t>(requests.size())) return;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(180);
        std::string replies;
        pollfd descriptor{fd, POLLIN, 0};
        while (replies.size() < 8192)
        {
            auto wait = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
            if (wait <= 0 || poll(&descriptor, 1, static_cast<int>(wait)) <= 0) break;
            char buffer[1024];
            auto n = read(fd, buffer, sizeof(buffer));
            if (n <= 0) break;
            replies.append(buffer, n);
            if (replies.find("\033]4;15;") != std::string::npos &&
                (replies.ends_with('\a') || replies.ends_with("\033\\"))) break;
        }
        for (int i = 0; i < 16; ++i)
        {
            const auto prefix = "\033]4;" + std::to_string(i) + ";rgb:";
            auto pos = replies.find(prefix);
            if (pos == std::string::npos) continue;
            pos += prefix.size();
            std::string rgb = "#";
            for (int component = 0; component < 3; ++component)
            {
                auto end = replies.find_first_not_of("0123456789abcdefABCDEF", pos);
                if (end == std::string::npos || end <= pos || end - pos > 4) break;
                if (component < 2 ? replies[end] != '/' : (replies[end] != '\a' && replies[end] != '\033')) break;
                rgb += replies.substr(pos, 1);
                rgb += replies[end - pos == 1 ? pos : pos + 1];
                pos = end + 1;
            }
            if (rgb.size() == 7) colors.hexValues[i] = rgb;
        }
    }

    inline void kitty(SystemInfo &info, const std::filesystem::path &config)
    {
        info.terminalInfo.name = "kitty";
        info.terminalInfo.version = versionOf("kitty --version", "kitty");
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
            paletteEntry(info.colorsInfo, key, value);
        }
        queryPalette(info.colorsInfo);
    }

    inline void collect(SystemInfo &info)
    {
        if (info.shellInfo.name == "Zsh")
            info.shellInfo.version = versionOf("zsh --version", "zsh");
        if (!env("SSH_CONNECTION").empty() || !env("SSH_TTY").empty()) return;
        const auto xdg = env("XDG_CONFIG_HOME");
        const std::filesystem::path config = xdg.empty() ? env("HOME") + "/.config" : xdg;
        if (env("TMUX").empty() && env("STY").empty() &&
            (info.terminalInfo.name == "Unknown" || info.terminalInfo.name == "kitty") &&
            (!env("KITTY_WINDOW_ID").empty() || env("TERM") == "xterm-kitty"))
            kitty(info, config);
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
        std::vector<DisplayInfo> displays;
        if (monitors.is_array())
            for (const auto &monitor : monitors)
            {
                if (!monitor.is_object()) continue;
                DisplayInfo display;
                display.name = monitor.value("name", std::string("Unknown"));
                display.id = monitor.value("id", -1);
                display.width = monitor.value("width", -1);
                display.height = monitor.value("height", -1);
                display.refreshRateHz = monitor.value("refreshRate", -1.0);
                display.platformApi = "Hyprland IPC";
                if (display.name.starts_with("eDP-")) display.type = "Builtin";
                displays.push_back(display);
            }
        if (!displays.empty()) info.displayInfo = std::move(displays);
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
