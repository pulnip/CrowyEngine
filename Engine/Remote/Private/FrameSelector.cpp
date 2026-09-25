#include "FrameSelector.hpp"

#include <algorithm>
#include <format>
#include <utility>

namespace Crowy
{
    namespace
    {
        FrameSelector failure(Str message) {
            return FrameSelector{.error = std::move(message)};
        }
    }

    std::optional<u64> parsePositiveInteger(const DOM::Value* value) {
        if(value == nullptr)
            return std::nullopt;

        const auto integer = value->asInt();
        if(!integer || *integer < 1)
            return std::nullopt;

        return static_cast<u64>(*integer);
    }

    FrameSelector parseFrameSelector(const DOM::Value& args) {
        constexpr auto BadList =
            "frames must be a non-empty array of positive integers";

        const auto* frame = args.at("frame");
        const auto* frames = args.at("frames");
        const auto* from = args.at("from");
        const auto* to = args.at("to");

        const bool range = from != nullptr || to != nullptr;
        const int forms = (frame != nullptr) + (frames != nullptr) + range;
        if(forms == 0)
            return {};
        if(forms > 1)
            return failure("use one of frame, frames, or from/to");

        const auto tooMany = std::format(
            "a selector names at most {} frames",
            MaxSelectedFrames
        );

        if(frame != nullptr) {
            const auto number = parsePositiveInteger(frame);
            if(!number)
                return failure("frame must be a positive integer");

            return FrameSelector{.frames = {*number}};
        }

        if(frames != nullptr) {
            const auto* list = frames->asArray();
            if(list == nullptr || list->empty())
                return failure(BadList);
            if(list->size() > MaxSelectedFrames)
                return failure(tooMany);

            std::vector<u64> selected;
            selected.reserve(list->size());
            for(const auto& item: *list) {
                const auto number = parsePositiveInteger(&item);
                if(!number)
                    return failure(BadList);
                if(std::ranges::find(selected, *number) != selected.end())
                    return failure("frames must not repeat");

                selected.push_back(*number);
            }

            return FrameSelector{.frames = std::move(selected)};
        }

        const auto first = parsePositiveInteger(from);
        const auto last = parsePositiveInteger(to);
        if(!first || !last)
            return failure("from and to must both be positive integers");
        if(*first > *last)
            return failure("from must not exceed to");
        if(*last - *first + 1 > MaxSelectedFrames)
            return failure(tooMany);

        std::vector<u64> selected;
        selected.reserve(*last - *first + 1);
        for(auto number = *first; number <= *last; ++number)
            selected.push_back(number);

        return FrameSelector{.frames = std::move(selected)};
    }
}
