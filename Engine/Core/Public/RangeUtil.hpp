#pragma once

#include <algorithm>
#include <functional>
#include <ranges>
#include <vector>

namespace Crowy
{
    // the elements of `keys` that `known` does not list, in their order
    template<std::ranges::input_range Keys, std::ranges::input_range Known>
    inline constexpr auto stray(Keys&& keys, const Known& known) {
        std::vector<std::ranges::range_value_t<Keys>> found;
        for(auto&& key: keys) {
            if(!std::ranges::contains(known, key))
                found.emplace_back(key);
        }

        return found;
    }

    template<std::ranges::forward_range R, typename Proj = std::identity>
    inline constexpr auto firstRepeat(R&& range, Proj proj = {}) {
        auto it = std::ranges::begin(range);
        for(; it != std::ranges::end(range); ++it) {
            const auto& key = std::invoke(proj, *it);
            if(std::ranges::contains(std::ranges::begin(range), it, key, proj))
                break;
        }

        return it;
    }
}
