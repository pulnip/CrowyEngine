#pragma once

#include <string>
#include <vector>
#include "ResourceHandle.hpp"

namespace Crowy
{
    struct MeshRequest{
        using Key     = std::string;
        using KeyHash = std::hash<std::string>;

        std::string uri;

        inline Key key() const{ return uri; }
    };

    struct MaterialSetRequest{
        using Key     = std::string;
        using KeyHash = std::hash<std::string>;

        std::string meshKey;
        // Pre-loaded material handles
        // std::vector<MaterialHandle> materialHandles;

        inline Key key() const{ return meshKey; }
    };

    struct ShaderRequest{
        using Key     = std::string;
        using KeyHash = std::hash<std::string>;

        std::string path;

        inline Key key() const{ return path; }
    };
}