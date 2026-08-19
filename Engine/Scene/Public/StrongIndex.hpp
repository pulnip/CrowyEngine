#pragma once

#include <functional>
#include <limits>
#include "Primitives.hpp"

namespace Crowy
{
    template<typename T>
    struct StrongIndex final{
        static constexpr usize INVALID = std::numeric_limits<usize>::max();

        usize value = INVALID;

        bool IsValid() const noexcept{
            return value != INVALID;
        }

        static StrongIndex Invalid() noexcept{
            return StrongIndex{INVALID};
        }

        friend auto operator<=>(StrongIndex, StrongIndex) noexcept = default;
    };
}

template<typename T>
struct std::hash<Crowy::StrongIndex<T>>{
    std::size_t operator()(Crowy::StrongIndex<T> index) const noexcept{
        return std::hash<Crowy::usize>{}(index.value);
    }
};
