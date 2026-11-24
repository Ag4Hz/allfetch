#include "allfetch/render/render.hpp"
#include "allfetch/render/logo_selector.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <filesystem>
#include <optional>
#include <ostream>
#include <utility>
#include <charconv>
#include <fstream>

namespace allfetch::render
{
    namespace
    {
        struct PrettyPalette
        {
            std::array<std::string, 16> hexValues{};

            explicit PrettyPalette(const system::ColorsInfo &colors)
            {
                const auto count = std::min(hexValues.size(), colors.hexValues.size());
                std::copy_n(colors.hexValues.begin(), count, hexValues.begin());
            }
        };

        constexpr std::string_view reset = "\x1b[0m";
        constexpr std::string_view accent = "\x1b[36m";
        constexpr std::string_view brightLogoAccent = "\x1b[96m";
        constexpr std::string_view white = "\x1b[37m";
        constexpr std::string_view swatch = "   ";
        constexpr std::size_t logoGap = 4;

        struct LogoRow
        {
            std::string text;
            std::size_t columns = 0;
        };

        struct InfoRow
        {
            std::string_view label;
            std::string value;
        };

        std::vector<LogoRow> loadLogoRows(
            const std::filesystem::path &path, bool useColor)
        {
            std::ifstream file(path);
            std::vector<LogoRow> rows;

            // First pass uses one accent. Later, give each slot its own style.
            const std::array<std::string_view, 9> styles{
                accent, accent, accent, accent, accent,
                accent, accent, accent, accent};
            auto style = styles.front();

            for (std::string line; std::getline(file, line);)
            {
                if (line.ends_with('\r'))
                    line.pop_back();

                LogoRow row;
                if (useColor && !line.empty())
                    row.text += style;

                for (std::size_t i = 0; i < line.size(); ++i)
                {
                    char c = line[i];

                    if (c == '$' && i + 1 < line.size())
                    {
                        const char marker = line[i + 1];

                        if (marker >= '1' && marker <= '9')
                        {
                            style = styles[marker - '1'];
                            if (useColor)
                                row.text += style;
                            ++i;
                            continue;
                        }

                        if (marker == '$')
                            ++i; // The first dollar is the visible character.
                    }

                    if (c == '\t')
                    {
                        row.text.append(4, ' ');
                        row.columns += 4;
                    }
                    else
                    {
                        row.text += c;
                        ++row.columns;
                    }
                }

                if (useColor && !row.text.empty())
                    row.text += reset;

                rows.push_back(std::move(row));
            }

            return rows;
        }

        void sanitizeRow(std::string &text)
        {
            for (char &c : text)
                if (static_cast<unsigned char>(c) < ' ' || c == '\x7f')
                    c = ' ';
        }

        std::vector<InfoRow> buildInfoRows(
            const system::SystemInfo &info, bool showAll, bool useColor)
        {
            std::vector<InfoRow> rows;

            const auto add = [&](std::string_view label, std::string value)
            {
                if (value.empty() || value == "Unknown")
                    return;

                // Keep each collected value on one physical row.
                sanitizeRow(value);

                rows.push_back({label, std::move(value)});
            };

            const auto withVersion = [](std::string name, const std::string &version)
            {
                if (!name.empty() && name != "Unknown" &&
                    !version.empty() && version != "Unknown")
                    name += " " + version;
                return name;
            };

            const auto &user = info.titleInfo.userName;
            const auto &host = info.titleInfo.hostName;
            const bool hasUser = !user.empty() && user != "Unknown";
            const bool hasHost = !host.empty() && host != "Unknown";
            if (hasUser || hasHost)
            {
                auto title = hasUser && hasHost ? user + "@" + host : (hasUser ? user : host);
                sanitizeRow(title);
                const auto length = std::ranges::count_if(title, [](unsigned char c)
                {
                    return (c & 0xc0) != 0x80; // Skip UTF-8 continuation bytes.
                });
                if (useColor)
                {
                    if (hasUser && hasHost)
                    {
                        title.insert(user.size() + 1, brightLogoAccent);
                        title.insert(user.size(), white);
                    }
                    title.insert(0, brightLogoAccent);
                    title += reset;
                }
                rows.push_back({{}, std::move(title)});
                rows.push_back({{}, std::string(static_cast<std::size_t>(length), '-')});
            }

            add("OS", withVersion(info.osInfo.name, info.osInfo.version));
            add("Host", info.hostInfo.name);
            add("Kernel", withVersion(info.osInfo.kernelName, info.osInfo.kernelVersion));
            add("Shell", withVersion(info.shellInfo.name, info.shellInfo.version));
            add("Terminal", withVersion(info.terminalInfo.name, info.terminalInfo.version));
            add("CPU", info.cpuInfo.model);

            for (const auto &gpu : info.gpuInfo)
                add("GPU", gpu.model);

            const auto total = info.memoryInfo.totalPhysicalMemory;
            const auto available = info.memoryInfo.availablePhysicalMemory;
            if (total > 0 && available >= 0 && available <= total)
            {
                constexpr long long mib = 1024 * 1024;
                add("Memory", std::to_string((total - available) / mib) + " MiB / " +
                                  std::to_string(total / mib) + " MiB");
            }

            if (showAll)
            {
                add("Architecture", info.osInfo.architecture);
                add("Window Manager", info.windowManagerInfo.name);
                add("Terminal Font", info.terminalInfo.font);
                add("Theme", info.desktopAppearanceInfo.theme);
                add("Locale", info.localeInfo.name);
            }

            return rows;
        }

        void writePaletteRow(std::ostream &output, const PrettyPalette &palette,
                             std::size_t start, bool trueColor)
        {
            for (std::size_t i = start; i < start + 8; ++i)
            {
                const auto &hex = palette.hexValues[i];
                if (trueColor && hex.size() == 7 && hex.front() == '#')
                {
                    unsigned rgb = 0;
                    const auto end = hex.data() + hex.size();
                    const auto parsed = std::from_chars(hex.data() + 1, end, rgb, 16);
                    if (parsed.ec == std::errc{} && parsed.ptr == end)
                    {
                        output << "\x1b[48;2;" << ((rgb >> 16) & 255) << ';'
                               << ((rgb >> 8) & 255) << ';' << (rgb & 255) << 'm' << swatch;
                        continue;
                    }
                }

                const auto code = i < 8 ? 40 + i : 100 + (i - 8);
                output << "\x1b[" << code << 'm' << swatch;
            }

            output << reset;
        }
    }

    void prettyRender(
        const system::SystemInfo &info,
        const LogoIndex &logoIndex,
        const std::filesystem::path &logoRoot,
        const PrettyOptions &options,
        const TerminalCapabilities &terminal,
        std::ostream &output)
    {
        {
            const bool useColor = options.colorEnabled && terminal.supportsAnsiColor;

            std::vector<LogoRow> logoRows;
            if (options.showLogo)
                if (const auto path = logoIndex.selectLogo(info.osInfo))
                    logoRows = loadLogoRows(logoRoot / std::filesystem::path(*path), useColor);

            const auto infoRows = buildInfoRows(info, options.showAll, useColor);

            std::optional<PrettyPalette> palette;
            if (useColor)
                palette.emplace(info.colorsInfo);

            std::size_t logoWidth = 0;
            for (const auto &row : logoRows)
                logoWidth = std::max(logoWidth, row.columns);

            const auto rightRows = infoRows.size() + (palette ? 3 : 0);
            const auto rowCount = std::max(logoRows.size(), rightRows);

            for (std::size_t i = 0; i < rowCount; ++i)
            {
                std::size_t currentWidth = 0;
                if (i < logoRows.size())
                {
                    output << logoRows[i].text;
                    currentWidth = logoRows[i].columns;
                }

                if (i < rightRows && i != infoRows.size())
                {
                    if (logoWidth > 0)
                        output << std::string(logoWidth - currentWidth + logoGap, ' ');

                    if (i < infoRows.size())
                    {
                        const auto &row = infoRows[i];
                        if (!row.label.empty())
                        {
                            if (useColor)
                                output << accent;
                            output << row.label;
                            if (useColor)
                                output << reset;
                            output << ": ";
                        }
                        output << row.value;
                    }
                    else
                    {
                        const auto start = (i - infoRows.size() - 1) * 8;
                        writePaletteRow(output, *palette, start, terminal.supportsTrueColor);
                    }
                }

                output << '\n';
            }
        }
    }
}
