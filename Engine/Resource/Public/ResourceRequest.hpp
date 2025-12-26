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

        std::string vsFilePath; // both binary and source file
        std::string vsFuncName;
        std::string fsFilePath; // both binary and source file
        std::string fsFuncName;

        inline Key key() const{
            return vsFilePath + ':' + vsFuncName + ',' +
                   fsFilePath + ':' + fsFuncName;
        }
    };
}