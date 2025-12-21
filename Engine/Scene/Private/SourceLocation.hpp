#pragma once

#include <cstddef>

namespace Crowy
{
    // for toml::source_region
    struct SourceLocation{
        size_t line, column; 
    };
}