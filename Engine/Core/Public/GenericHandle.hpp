#pragma once

#include <limits>
#include "HashUtil.hpp"
#include "Primitives.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    template<typename T>
    struct GenericHandle final{
    public:
        static constexpr auto INVALID_INDEX = std::numeric_limits<usize>::max();

    private:
        usize index = INVALID_INDEX;
        usize generation = 0;

    public:
        GenericHandle() noexcept
            : index(INVALID_INDEX), generation(0){}
        GenericHandle(usize index, usize generation) noexcept
            : index(index), generation(generation){}
        ~GenericHandle() = default;
        CROWY_DECLARE_TRANSFERABLE_NOEXCEPT(GenericHandle)

        // Notice! This doesn't mean pointed slot (in Container) is valid.
        bool IsValid() const noexcept{
            return index != INVALID_INDEX;
        }

        usize GetIndex() const noexcept{ return index; }
        usize GetGeneration() const noexcept{ return generation; }

        static auto InvalidHandle() noexcept{
            return GenericHandle(INVALID_INDEX, 0);
        }

        friend bool operator==(GenericHandle lhs, GenericHandle rhs) noexcept{
            return lhs.index == rhs.index && lhs.generation == rhs.generation;
        }
    };
}

template<typename T>
struct std::hash<Crowy::GenericHandle<T>>{
    std::size_t operator()(const Crowy::GenericHandle<T>& handle) const noexcept{
        using namespace Crowy;

        return Crowy::hashAll(
            handle.GetIndex(),
            handle.GetGeneration()
        );
    }
};
