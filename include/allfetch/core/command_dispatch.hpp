#pragma once

#include "argparser.hpp"

namespace allfetch::core
{

    // Function to dispatch the command based on the parsed arguments
    bool dispatchCommand(const allfetch::core::ParserResult &result);

}