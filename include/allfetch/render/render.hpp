#pragma once

#include "allfetch/system/system_info.hpp"
#include <filesystem>
#include <iosfwd>

namespace allfetch::render
{
    class LogoIndex;

    struct PrettyOptions
    {
        bool colorEnabled = true;
        bool showLogo = true;
        bool showAll = false;
    };

    struct TerminalCapabilities
    {
        bool supportsAnsiColor = false;
        bool supportsTrueColor = false;
    };

    void renderJson(
        const system::SystemInfo &info,
        std::ostream &output);

    void prettyRender(
        const system::SystemInfo &info,
        const LogoIndex &logoIndex,
        const std::filesystem::path &logoRoot,
        const PrettyOptions &options,
        const TerminalCapabilities &terminal,
        std::ostream &output);
}
