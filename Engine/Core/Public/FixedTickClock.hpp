#pragma once

#include <algorithm>
#include <chrono>
#include <ratio>

#include "Primitives.hpp"

namespace Crowy
{
    // 1/60 s rounded once to whole nanoseconds, as App::FixedStep rounds it
    inline constexpr i64 TickNanoseconds =
        std::chrono::round<std::chrono::nanoseconds>(
            std::chrono::duration<i64, std::ratio<1, 60>>(1)
        )
            .count();
    // what a frame owes beyond this many ticks is dropped
    inline constexpr u32 MaxTicksPerAdvance = 4;

    static_assert(TickNanoseconds == 16'666'667);

    // seconds that were whole nanoseconds back to those nanoseconds; 0 for a
    // clock that ran backwards
    inline constexpr i64 toNanoseconds(f64 seconds) noexcept {
        return seconds > 0.0 ? static_cast<i64>(seconds * 1e9 + 0.5) : 0;
    }

    // Frame time to fixed ticks, in integers so nothing drifts; the carry
    // stays below one tick.
    class FixedTickClock {
    private:
        i64 carry = 0;

    public:
        constexpr u32 Advance(i64 frameNanoseconds) noexcept {
            const auto total = carry + std::max<i64>(frameNanoseconds, 0);
            carry = total % TickNanoseconds;

            return static_cast<u32>(
                std::min<i64>(total / TickNanoseconds, MaxTicksPerAdvance)
            );
        }

        constexpr i64 Carry() const noexcept { return carry; }
    };
}
