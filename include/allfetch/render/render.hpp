#pragma once

#include "allfetch/system/system_info.hpp"
#include <iosfwd>

namespace allfetch::render
{
    void renderJson(
        const system::SystemInfo &info,
        std::ostream &output);
}