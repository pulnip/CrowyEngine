#pragma once

#include <algorithm>
#include <ranges>
#include <vector>

namespace Crowy
{
    // the elements of `keys` that `known` does not list, in their order
    template<std::ranges::input_range Keys, std::ranges::input_range Known>
    inline constexpr auto stray(Keys&& keys, const Known& known) {
        std::vector<std::ranges::range_value_t<Keys>> found;
        for(auto&& key: keys) {
            if(std::ranges::find(known, key) == std::ranges::end(known))
                found.emplace_back(key);
        }

        return found;
    }
}
