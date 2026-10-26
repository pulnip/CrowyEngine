#pragma once

#include <array>
#include <utility>

#include "Assert.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    template<typename T, usize N>
    class FrameHistory {
    public:
        static constexpr usize Depth = N;

    private:
        struct Entry {
            u64 frame = 0;
            T value{};
        };

        std::array<Entry, N> entries{};
        u64 newest = 0;

    public:
        void Push(u64 frame, T value) {
            CROWY_ASSERT(
                frame > newest,
                "frame {} pushed after frame {}: frames arrive in order",
                frame,
                newest
            );

            entries[frame % N] =
                Entry{.frame = frame, .value = std::move(value)};
            newest = frame;
        }

        const T* Find(u64 frame) const noexcept {
            const auto& entry = entries[frame % N];

            return frame != 0 && entry.frame == frame ? &entry.value : nullptr;
        }

        u64 Newest() const noexcept { return newest; }

        u64 Oldest() const noexcept {
            if(newest == 0)
                return 0;

            return newest > N ? newest - N + 1 : 1;
        }
    };
}
