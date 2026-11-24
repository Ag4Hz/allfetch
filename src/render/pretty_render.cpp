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
#include <cmath>
#include <numeric>

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
        constexpr std::string_view brightGreen = "\x1b[92m";
        constexpr std::string_view brightYellow = "\x1b[93m";
        constexpr std::string_view brightRed = "\x1b[91m";
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
            std::string label;
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

        constexpr long long bytesPerMiB = 1024LL * 1024;

        std::string decimal(double value, int precision)
        {
            char text[32];
            const auto result = std::to_chars(text, text + sizeof(text),
                value, std::chars_format::fixed, precision);
            return result.ec == std::errc{} ? std::string(text, result.ptr) : std::string{};
        }

        void appendClock(std::string &value, double mhz)
        {
            if (!std::isfinite(mhz) || mhz <= 0)
                return;
            if (mhz >= 1000)
                value += " @ " + decimal(mhz / 1000, 2) + " GHz";
            else
                value += " @ " + decimal(mhz, 0) + " MHz";
        }

        std::string formatSize(long long bytes)
        {
            constexpr long long bytesPerGiB = 1024 * bytesPerMiB;
            const bool useGiB = bytes >= bytesPerGiB;
            const auto unit = useGiB ? bytesPerGiB : bytesPerMiB;
            return decimal(static_cast<double>(bytes) / unit, 2) + (useGiB ? " GiB" : " MiB");
        }

        std::string formatUsage(long long used, long long total)
        {
            if (total < 0)
                return used < 0 ? "Unknown" : formatSize(used) + " used";
            if (used < 0 || used > total)
                return formatSize(total) + " total";
            return formatSize(used) + " / " + formatSize(total);
        }

        std::optional<int> usagePercent(long long used, long long total)
        {
            if (total <= 0 || used < 0 || used > total)
                return std::nullopt;
            return static_cast<int>(std::lround(100.0 * used / total));
        }

        enum class PercentKind { Usage, BatteryCharge };

        std::string formatPercent(int percent, PercentKind kind, bool useColor)
        {
            std::string text;
            if (useColor)
            {
                if (kind == PercentKind::BatteryCharge)
                    text += percent < 50 ? brightRed : (percent < 75 ? brightYellow : brightGreen);
                else
                    text += percent < 50 ? brightGreen : (percent < 75 ? brightYellow : brightRed);
            }
            text += std::to_string(percent) + '%';
            if (useColor)
                text += reset;
            return text;
        }

        bool known(std::string_view value)
        {
            return !value.empty() && value != "Unknown";
        }

        std::string deviceLabel(std::string_view label, std::string_view device)
        {
            std::string result(label);
            if (known(device))
            {
                result += " (";
                result += device;
                result += ')';
            }
            return result;
        }

        std::vector<InfoRow> buildInfoRows(
            const system::SystemInfo &info, bool showAll, bool useColor)
        {
            std::vector<InfoRow> rows;

            const auto add = [&](std::string label, std::string value,
                                 std::optional<int> percent = std::nullopt,
                                 PercentKind kind = PercentKind::Usage)
            {
                if (!known(value) && !percent)
                    return;

                // Interface and disk labels can contain system-provided text.
                sanitizeRow(label);
                sanitizeRow(value);

                if (percent)
                {
                    auto formatted = formatPercent(*percent, kind, useColor);
                    if (kind == PercentKind::BatteryCharge)
                        value = value.empty() ? std::move(formatted) : formatted + " [" + value + ']';
                    else
                        value += " (" + formatted + ')';
                }
                rows.push_back({std::move(label), std::move(value)});
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

            auto os = withVersion(info.osInfo.name, info.osInfo.version);
            if (known(os) && known(info.osInfo.architecture))
                os += " " + info.osInfo.architecture;
            add("OS", std::move(os));

            auto hostName = info.hostInfo.name;
            if (known(hostName) && known(info.hostInfo.version))
                hostName += " (" + info.hostInfo.version + ')';
            add("Host", std::move(hostName));
            add("Kernel", withVersion(info.osInfo.kernelName, info.osInfo.kernelVersion));
            add("Shell", withVersion(info.shellInfo.name, info.shellInfo.version));
            for (const auto &display : info.displayInfo)
            {
                std::string value;
                if (display.width > 0 && display.height > 0)
                {
                    const auto divisor = std::gcd(display.width, display.height);
                    const auto aspectWidth = display.width / divisor;
                    const auto aspectHeight = display.height / divisor;
                    // Use the conventional display label for an 8:5 ratio.
                    const auto aspect = aspectWidth == 8 && aspectHeight == 5
                        ? std::string("16:10")
                        : std::to_string(aspectWidth) + ':' + std::to_string(aspectHeight);
                    value = std::to_string(display.width) + 'x' + std::to_string(display.height);
                    value += " (" + aspect + ')';
                }
                if (std::isfinite(display.refreshRateHz) && display.refreshRateHz > 0)
                {
                    if (!value.empty())
                        value += " @ ";
                    value += decimal(display.refreshRateHz, 0) + " Hz";
                }
                if (known(display.type))
                {
                    if (!value.empty())
                        value += ' ';
                    value += '[' + display.type + ']';
                }
                add(deviceLabel("Display", display.name),
                    value.empty() ? "Mode unavailable" : std::move(value));
            }
            add("Terminal", withVersion(info.terminalInfo.name, info.terminalInfo.version));

            const auto &cpu = info.cpuInfo;
            if (known(cpu.model) || cpu.physicalCores > 0 || cpu.logicalCores > 0 ||
                cpu.maxFrequency > 0 || cpu.baseFrequency > 0 || cpu.reportedFrequency > 0)
            {
                auto value = known(cpu.model) ? cpu.model : "Unknown model";
                if (cpu.physicalCores > 0 && cpu.logicalCores > 0)
                    value += " (" + std::to_string(cpu.physicalCores) + " + " +
                             std::to_string(cpu.logicalCores) + ')';
                else if (cpu.physicalCores > 0)
                    value += " (" + std::to_string(cpu.physicalCores) + " physical)";
                else if (cpu.logicalCores > 0)
                    value += " (" + std::to_string(cpu.logicalCores) + " logical)";
                appendClock(value, cpu.maxFrequency > 0 ? cpu.maxFrequency :
                    (cpu.baseFrequency > 0 ? cpu.baseFrequency : cpu.reportedFrequency));
                add("CPU", std::move(value));
            }

            std::size_t gpuNumber = 0;
            for (const auto &gpu : info.gpuInfo)
            {
                std::string value = known(gpu.model) ? gpu.model : "Unknown model";
                if (gpu.memorySizeMB >= 0)
                    value += " (" + formatSize(gpu.memorySizeMB * bytesPerMiB) + ')';
                if (known(gpu.type))
                    value += " [" + gpu.type + ']';
                appendClock(value, gpu.maxFrequencyMHz);
                add("GPU " + std::to_string(++gpuNumber), std::move(value));
            }

            const auto total = info.memoryInfo.totalPhysicalMemory;
            const auto available = info.memoryInfo.availablePhysicalMemory;
            const auto used = total >= 0 && available >= 0 && available <= total
                ? total - available : -1;
            add("Memory", formatUsage(used, total), usagePercent(used, total));
            if (info.swapInfo.totalBytes > 0 || info.swapInfo.usedBytes > 0)
                add("Swap", formatUsage(info.swapInfo.usedBytes, info.swapInfo.totalBytes),
                    usagePercent(info.swapInfo.usedBytes, info.swapInfo.totalBytes));

            for (const auto &disk : info.diskInfo)
            {
                if (std::ranges::find(disk.volumeType, "Network") != disk.volumeType.end())
                    continue;

                auto value = formatUsage(disk.usedBytes, disk.totalBytes);
                if (known(disk.filesystem))
                    value = known(value) ? value + " - " + disk.filesystem : disk.filesystem;
                if (!known(value))
                    value = "Capacity unavailable";
                add(deviceLabel("Disk", disk.mountpoint), std::move(value),
                    usagePercent(disk.usedBytes, disk.totalBytes));
            }

            for (const auto &ip : info.localIPInfo)
            {
                if (!known(ip.address) || (ip.interfaceType != "Ethernet" && ip.interfaceType != "Wi-Fi"))
                    continue;

                auto address = ip.address;
                if (ip.prefixLength >= 0 && ip.prefixLength <= 32)
                    address += '/' + std::to_string(ip.prefixLength);
                const auto &interface = known(ip.interfaceName) && !ip.interfaceName.starts_with('{')
                    ? ip.interfaceName : ip.interfaceType;
                add(deviceLabel("Local IP", interface), std::move(address));
            }

            for (const auto &battery : info.batteryInfo)
            {
                std::optional<int> charge;
                if (battery.capacityPercent >= 0 && battery.capacityPercent <= 100)
                    charge = static_cast<int>(std::lround(battery.capacityPercent));

                std::string status;
                for (const auto &item : battery.status)
                {
                    if (!known(item))
                        continue;
                    if (!status.empty())
                        status += ", ";
                    status += item;
                }
                if (status.empty() && !charge)
                    status = "Present";
                add(deviceLabel("Battery", battery.modelName), std::move(status),
                    charge, PercentKind::BatteryCharge);
            }

            if (showAll)
            {
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
