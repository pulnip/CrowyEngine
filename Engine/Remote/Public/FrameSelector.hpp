#pragma once

#include <optional>
#include <vector>

#include "DOM.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    inline constexpr usize MaxSelectedFrames = 64;

    struct FrameSelector {
        std::vector<u64> frames;
        Str error;
    };

    std::optional<u64> parsePositiveInteger(const DOM::Value*);

    FrameSelector parseFrameSelector(const DOM::Value& args);
    std::vector<Str> withSelectorKeys(std::vector<Str> keys);
}
