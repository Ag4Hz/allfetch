#pragma once

#include "allfetch/system/system_info.hpp"

#include <cstdlib>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace allfetch::system
{
    inline LocaleInfo getLocaleInfo()
    {
        LocaleInfo info;

#if defined(_WIN32)
        wchar_t name[LOCALE_NAME_MAX_LENGTH]{};

        const int length = GetUserDefaultLocaleName(
            name, LOCALE_NAME_MAX_LENGTH);

        if (length <= 0)
            return info;

        if (length == 1)
        {
            info.name = "Invariant";
            return info;
        }

        const int bytes = WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS,
            name, length - 1,
            nullptr, 0, nullptr, nullptr);

        if (bytes <= 0)
            return info;

        std::string result(bytes, '\0');

        if (WideCharToMultiByte(
                CP_UTF8, WC_ERR_INVALID_CHARS,
                name, length - 1,
                result.data(), bytes,
                nullptr, nullptr))
        {
            info.name = result;
        }

#elif defined(__linux__)
        // Report the configured message locale.
        for (const char *variable : {"LC_ALL", "LC_MESSAGES", "LANG"})
        {
            const char *value = std::getenv(variable);

            if (value != nullptr && value[0] != '\0')
            {
                info.name = value;
                return info;
            }
        }

        info.name = "C";
#endif

        return info;
    }
}