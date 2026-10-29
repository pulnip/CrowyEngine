#pragma once

#include <vector>

#include "DOM.hpp"

namespace Crowy
{
    template<typename T>
    struct DomTraits;

    // why root is not a `type` document of `version`, none when it is; its
    // metadata may hold version, type and name only
    [[nodiscard]] std::vector<Str> checkDocMetadata(
        const DOM::Value& root,
        StrView type,
        i64 version
    );

    struct DocMetadata {
        u32 version = 0;
        Str type = "Unknown";
        Str name = "Unnamed";
    };
    template<>
    struct DomTraits<DocMetadata> {
        static DocMetadata from(const DOM::Value& root);
    };
}
