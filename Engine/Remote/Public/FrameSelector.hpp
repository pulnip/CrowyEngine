#pragma once

#include <optional>
#include <vector>

#include "DOM.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    inline constexpr usize MaxSelectedFrames = 64;

    // {"frame": F} | {"frames": [F, ...]} | {"from": a, "to": b}; frames
    // count from 1
    struct FrameSelector {
        // in request order; empty when the args carry no selector
        std::vector<u64> frames;
        // the wire message for a malformed one; frames is then empty
        Str error;
    };

    // a frame number or a frame count on the wire: an integer from 1 up
    std::optional<u64> parsePositiveInteger(const DOM::Value*);

    FrameSelector parseFrameSelector(const DOM::Value& args);
}
