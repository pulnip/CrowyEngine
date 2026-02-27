#pragma once

#include <string>
#include <utility>

namespace Crowy
{
    enum class SchemeKind{
        File     = 0,
        Embedded = 1,
        Unknown  = 2
    };

    // "scheme:path" -> {scheme, path}
    inline std::pair<SchemeKind, std::string> splitSchemeAndPath(const std::string& uri){
        auto colon_pos = uri.find(':');
        if(colon_pos == std::string::npos)
            return {SchemeKind::Unknown, ""};

        auto schemeStr = uri.substr(0, colon_pos);
        auto pathStr = uri.substr(colon_pos + 1);

        if(schemeStr == "file")
            return {SchemeKind::File, pathStr};
        else if(schemeStr == "embedded")
            return {SchemeKind::Embedded, pathStr};
        else
            return {SchemeKind::Unknown, pathStr};
    }
}