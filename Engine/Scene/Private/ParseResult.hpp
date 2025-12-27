#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include "SourceLocation.hpp"
#include "SceneSpec.hpp"

namespace Crowy
{
    struct VNull{
        SourceLocation location;
    };
    struct VBool{
        bool v;
        SourceLocation location;
    };
    struct VInt{
        int64_t v;
        SourceLocation location;
    };
    struct VFloat{
        double v;
        SourceLocation location;
    };
    struct VString{
        std::string v;
        SourceLocation location;
    };
    struct VArray{
        std::vector<size_t> elements;
        SourceLocation location;
    };
    struct VTable{
        std::unordered_map<std::string, size_t> fields;
        SourceLocation location;
    };

    using VNode = std::variant<
        VNull, VBool, VInt, VFloat,
        VString, VArray, VTable>;

    struct ValueArena{
        std::vector<VNode> nodes;
        inline size_t emplace(VNode n){
            nodes.push_back(std::move(n));
            return nodes.size() - 1;
        }
    };

    // parse result
    struct ParseElement{
        std::string name;
        size_t index = std::numeric_limits<size_t>::max();
        SourceLocation location;
    };
    struct ParseResult{
        ValueArena arena;
        std::vector<ParseElement> elements;
    };
}