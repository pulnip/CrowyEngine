#pragma once

#include <string>
#include <vector>
#include "ResourceHandle.hpp"

namespace Crowy
{
    struct ModelRequest{
        std::string uri;
    };

    struct ShaderRequest{
        using Key     = std::string;
        using KeyHash = std::hash<std::string>;

        std::string path;

        inline Key key() const{ return path; }
    };
}