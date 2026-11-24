#pragma once

#include "allfetch/system/system_info.hpp"

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#elif defined(__linux__)
#include <filesystem>
#include <fstream>
#include <ifaddrs.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <netinet/in.h>
#endif

namespace allfetch::system
{
    namespace local_ip_detail
    {
        inline std::string ipv4Text(const void *address)
        {
            // IPv4 bytes are already stored in network order.
            const auto *bytes =
                static_cast<const unsigned char *>(address);

            return std::to_string(bytes[0]) + "." +
                   std::to_string(bytes[1]) + "." +
                   std::to_string(bytes[2]) + "." +
                   std::to_string(bytes[3]);
        }

        inline bool usableAddress(const void *address)
        {
            const auto *bytes =
                static_cast<const unsigned char *>(address);

            return bytes[0] != 0 &&
                   bytes[0] != 127 &&
                   bytes[0] < 224;
        }

#if defined(__linux__)
        inline std::string interfaceType(const char *name)
        {
            if (name == nullptr)
                return "Unknown";

            const auto path = std::filesystem::path("/sys/class/net") / name;
            std::error_code error;
            if (std::filesystem::exists(path / "phy80211", error) ||
                std::filesystem::exists(path / "wireless", error))
                return "Wi-Fi";

            int hardwareType = 0;
            std::ifstream typeFile(path / "type");
            if (typeFile >> hardwareType && hardwareType == ARPHRD_ETHER)
                return "Ethernet";
            return "Unknown";
        }

        inline std::optional<int> prefixFromMask(const void *mask)
        {
            const auto *bytes =
                static_cast<const unsigned char *>(mask);

            int prefix = 0;
            bool foundZero = false;

            for (int index = 0; index < 4; ++index)
            {
                for (int bit = 7; bit >= 0; --bit)
                {
                    if ((bytes[index] & (1u << bit)) != 0)
                    {
                        if (foundZero)
                        {
                            return std::nullopt;
                        }

                        ++prefix;
                    }
                    else
                    {
                        foundZero = true;
                    }
                }
            }

            return prefix;
        }
#endif
    }

    inline std::vector<LocalIPInfo> getLocalIPInfo()
    {
        std::vector<LocalIPInfo> result;

#if defined(_WIN32)
        ULONG bytes = 15000;
        constexpr ULONG flags =
            GAA_FLAG_SKIP_ANYCAST |
            GAA_FLAG_SKIP_MULTICAST |
            GAA_FLAG_SKIP_DNS_SERVER;

        for (int attempt = 0; attempt < 3; ++attempt)
        {
            std::unique_ptr<IP_ADAPTER_ADDRESSES, decltype(&std::free)>
                adapters(
                    static_cast<IP_ADAPTER_ADDRESSES *>(
                        std::malloc(bytes)),
                    &std::free);

            if (!adapters)
            {
                return result;
            }

            const ULONG status = GetAdaptersAddresses(
                AF_INET, flags, nullptr, adapters.get(), &bytes);

            if (status == ERROR_BUFFER_OVERFLOW)
            {
                continue;
            }

            if (status != NO_ERROR)
            {
                return result;
            }

            for (auto *adapter = adapters.get();
                 adapter != nullptr;
                 adapter = adapter->Next)
            {
                if (adapter->OperStatus != IfOperStatusUp ||
                    adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
                {
                    continue;
                }

                for (auto *entry = adapter->FirstUnicastAddress;
                     entry != nullptr;
                     entry = entry->Next)
                {
                    if (entry->Address.lpSockaddr == nullptr ||
                        entry->Address.lpSockaddr->sa_family != AF_INET ||
                        entry->OnLinkPrefixLength > 32 ||
                        (entry->DadState != IpDadStatePreferred &&
                         entry->DadState != IpDadStateDeprecated))
                    {
                        continue;
                    }

                    const auto *address =
                        reinterpret_cast<const sockaddr_in *>(
                            entry->Address.lpSockaddr);

                    if (!local_ip_detail::usableAddress(
                            &address->sin_addr))
                    {
                        continue;
                    }

                    LocalIPInfo info;

                    // AdapterName is the adapter identifier.
                    info.interfaceName = adapter->AdapterName != nullptr
                                             ? adapter->AdapterName
                                             : "Unknown";

                    info.address = local_ip_detail::ipv4Text(
                        &address->sin_addr);

                    info.prefixLength = entry->OnLinkPrefixLength;
                    if (adapter->IfType == IF_TYPE_ETHERNET_CSMACD)
                        info.interfaceType = "Ethernet";
                    else if (adapter->IfType == IF_TYPE_IEEE80211)
                        info.interfaceType = "Wi-Fi";
                    result.push_back(info);
                }
            }

            break;
        }

#elif defined(__linux__)
        ifaddrs *rawInterfaces = nullptr;

        if (getifaddrs(&rawInterfaces) != 0)
        {
            return result;
        }

        std::unique_ptr<ifaddrs, decltype(&freeifaddrs)>
            interfaces(rawInterfaces, &freeifaddrs);

        for (auto *entry = interfaces.get();
             entry != nullptr;
             entry = entry->ifa_next)
        {
            if (entry->ifa_addr == nullptr ||
                entry->ifa_netmask == nullptr ||
                entry->ifa_addr->sa_family != AF_INET ||
                entry->ifa_netmask->sa_family != AF_INET ||
                !(entry->ifa_flags & IFF_UP) ||
                !(entry->ifa_flags & IFF_RUNNING) ||
                (entry->ifa_flags & IFF_LOOPBACK))
            {
                continue;
            }

            const auto *address =
                reinterpret_cast<const sockaddr_in *>(entry->ifa_addr);

            const auto *mask =
                reinterpret_cast<const sockaddr_in *>(entry->ifa_netmask);

            if (!local_ip_detail::usableAddress(&address->sin_addr))
            {
                continue;
            }

            const auto prefix = local_ip_detail::prefixFromMask(
                &mask->sin_addr);

            if (!prefix)
            {
                continue;
            }

            LocalIPInfo info;
            info.interfaceName = entry->ifa_name != nullptr
                                     ? entry->ifa_name
                                     : "Unknown";

            info.address = local_ip_detail::ipv4Text(
                &address->sin_addr);

            info.prefixLength = *prefix;
            info.interfaceType = local_ip_detail::interfaceType(entry->ifa_name);
            result.push_back(info);
        }
#endif

        return result;
    }
}
