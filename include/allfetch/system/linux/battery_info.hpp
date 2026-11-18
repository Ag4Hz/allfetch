#pragma once

#include "allfetch/system/system_info.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace allfetch::system
{
    namespace battery_detail
    {
        namespace fs = std::filesystem;

        inline std::string text(const fs::path &path)
        {
            std::ifstream file(path);
            std::string value;
            std::getline(file, value);

            while (!value.empty() && (value.back() == '\r' || value.back() == ' ' || value.back() == '\t'))
            {
                value.pop_back();
            }

            return value;
        }

        inline std::optional<double> number(const fs::path &path)
        {
            std::ifstream file(path);
            double value = 0;

            if (file >> value && std::isfinite(value))
                return value;

            return std::nullopt;
        }

        inline void readString(
            const fs::path &path,
            std::string &destination)
        {
            const std::string value = text(path);

            if (!value.empty())
                destination = value;
        }

        inline std::optional<double> ratio(
            const fs::path &path,
            const char *currentName,
            const char *fullName)
        {
            const auto current = number(path / currentName);
            const auto full = number(path / fullName);

            if (!current || !full || *current < 0 || *full <= 0)
                return std::nullopt;

            const double value = *current / *full;

            if (std::isfinite(value))
                return value;

            return std::nullopt;
        }

        inline BatteryInfo collect(
            const fs::path &path,
            bool externalPower)
        {
            BatteryInfo info;

            readString(path / "manufacturer", info.manufacturer);
            readString(path / "model_name", info.modelName);
            readString(path / "serial_number", info.serial);

            // Linux reports chemistry, such as "Li-ion".
            readString(path / "technology", info.technology);

            const auto cycles = number(path / "cycle_count");

            if (cycles && *cycles >= 0 && *cycles <= std::numeric_limits<int>::max())
            {
                info.cycleCount = static_cast<int>(*cycles);
            }

            // Prefer energy/charge ratios to retain decimal precision.
            auto fraction = ratio(path, "energy_now", "energy_full");

            if (!fraction)
                fraction = ratio(path, "charge_now", "charge_full");

            if (fraction)
            {
                info.capacityPercent = *fraction * 100.0;
            }
            else
            {
                const auto capacity = number(path / "capacity");

                if (capacity && *capacity >= 0 && *capacity <= 100)
                    info.capacityPercent = *capacity;
            }

            const auto temperature = number(path / "temp");

            if (temperature)
                info.temperatureCelsius = *temperature / 10.0;

            const std::string state = text(path / "status");

            if (externalPower)
                info.status.emplace_back("AC Connected");

            if (!state.empty() && state != "Unknown")
                info.status.push_back(state);

            if (text(path / "capacity_level") == "Critical")
                info.status.emplace_back("Critical");

            if (state == "Discharging")
            {
                auto seconds = number(path / "time_to_empty_now");

                if (!seconds || *seconds < 0)
                {
                    auto hours =
                        ratio(path, "energy_now", "power_now");

                    if (!hours)
                        hours =
                            ratio(path, "charge_now", "current_now");

                    if (hours)
                        seconds = *hours * 3600.0;
                }

                if (seconds && std::isfinite(*seconds) && *seconds >= 0 && *seconds < static_cast<double>(std::numeric_limits<long long>::max()))
                {
                    info.timeRemainingSeconds =
                        static_cast<long long>(*seconds);
                }
            }

            const auto year = number(path / "manufacture_year");
            const auto month = number(path / "manufacture_month");
            const auto day = number(path / "manufacture_day");

            if (year && month && day && *year >= 1900 && *year <= 9999 && *month >= 1 && *month <= 12 && *day >= 1 && *day <= 31)
            {
                char date[16]{};

                std::snprintf(
                    date, sizeof(date), "%04d-%02d-%02d",
                    static_cast<int>(*year),
                    static_cast<int>(*month),
                    static_cast<int>(*day));

                info.manufactureDate = date;
            }

            return info;
        }
    }

    inline std::vector<BatteryInfo> getBatteryInfo()
    {
        namespace fs = std::filesystem;

        std::vector<BatteryInfo> result;
        std::vector<fs::path> batteries;
        bool externalPower = false;

        std::error_code error;
        fs::directory_iterator entry(
            "/sys/class/power_supply", error);
        const fs::directory_iterator end;

        while (!error && entry != end)
        {
            const fs::path path = entry->path();
            const std::string type =
                battery_detail::text(path / "type");

            if (type == "Battery")
            {
                const auto present =
                    battery_detail::number(path / "present");

                const bool deviceBattery =
                    battery_detail::text(path / "scope") == "Device";

                if ((!present || *present != 0) && !deviceBattery)
                    batteries.push_back(path);
            }
            else if (type == "Mains" || type.starts_with("USB") || type == "Wireless")
            {
                const auto online =
                    battery_detail::number(path / "online");

                if (online && *online == 1)
                    externalPower = true;
            }

            entry.increment(error);
        }

        for (const auto &path : batteries)
        {
            result.push_back(
                battery_detail::collect(path, externalPower));
        }

        return result;
    }
}