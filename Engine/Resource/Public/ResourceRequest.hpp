#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include "path_util.hpp"

namespace Crowy
{
    struct ModelRequest{
        std::string uri;
    };

    struct ShaderRequest{
        using Key     = std::string;
        using KeyHash = std::hash<std::string>;

        std::filesystem::path vsFilePath; // both binary and source file
        std::string vsFuncName;
        std::filesystem::path fsFilePath; // both binary and source file
        std::string fsFuncName;

        inline Key key() const{
            return to_utf8String(vsFilePath) + ':' + vsFuncName + ',' +
                   to_utf8String(fsFilePath) + ':' + fsFuncName;
        }
    };
}