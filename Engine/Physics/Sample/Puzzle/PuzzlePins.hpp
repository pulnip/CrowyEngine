#pragma once

#include <array>
#include <span>

#include "Primitives.hpp"
#include "Puzzle.hpp"

namespace Crowy
{
    struct PuzzlePin;

    // tick 0 pins the build itself; the rest pin the run
    inline constexpr std::array<u64, 6> PinTicks{0, 1, 10, 60, 240, 600};

    std::span<const PuzzlePin> puzzlePins();
    // null when the puzzle and mode have no row
    const PuzzlePin* findPin(PuzzleKind kind, PuzzleMode mode);
    // the spelling the table and the port use
    Str formatHash(u64 hash);
    // a row exactly as the table writes it, to paste over the old one
    Str formatPin(const PuzzlePin& pin);
    // the first difference in words, empty when the two agree
    Str firstDivergence(const PuzzlePin& pinned, const PuzzlePin& actual);

    // a run's numbers at the pinned ticks
    struct PuzzlePin {
        PuzzleKind kind = PuzzleKind::KickerJump;
        PuzzleMode mode = PuzzleMode::Solution;
        // 0: never solved within the horizon
        u64 solvedAt = 0;
        std::array<u64, PinTicks.size()> hashes{};

        friend bool operator==(const PuzzlePin&, const PuzzlePin&) = default;
    };
}
