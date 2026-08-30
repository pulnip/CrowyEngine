#pragma once

#include "DOM.hpp"

namespace Crowy
{
    template<typename T>
    struct DomTraits;

    struct DocMetadata{
        u32 version = 0;
        Str type = "Unknown";
        Str name = "Unnamed";
    };
    template<>
    struct DomTraits<DocMetadata>{
        static DocMetadata from(const DOM::Value& root);
    };
}
