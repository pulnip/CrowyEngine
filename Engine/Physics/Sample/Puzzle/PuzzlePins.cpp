#include "PuzzlePins.hpp"

#include <algorithm>
#include <format>

namespace
{
    using Crowy::PuzzleKind;
    using Crowy::PuzzleMode;
    using Crowy::PuzzlePin;

    // pasted from PuzzlePins.MatchTheTable's output, never edited by hand
    // clang-format off
    constexpr std::array<PuzzlePin, 2> PinTable{{
    PuzzlePin{PuzzleKind::SwingCut, PuzzleMode::Solution, 77, {
        0x2bf0a4375d246b1c, 0x02a28a66c96b6f10, 0x08e941ba9cee7761,
        0x2ce13cd8fcce2414, 0x942d9115cb21a837, 0x40de5201cca89c59,
    }},
    PuzzlePin{PuzzleKind::SwingCut, PuzzleMode::Control, 0, {
        0x2bf0a4375d246b1c, 0x02a28a66c96b6f10, 0x08e941ba9cee7761,
        0xd8630d2b951f901b, 0x92e38a51e11a569d, 0x8a1ad0e78f35ae90,
    }},
    }};
    // clang-format on
}

namespace Crowy
{
    std::span<const PuzzlePin> puzzlePins() {
        return PinTable;
    }

    const PuzzlePin* findPin(PuzzleKind kind, PuzzleMode mode) {
        const auto found = std::ranges::find_if(PinTable, [&](const auto& pin) {
            return pin.kind == kind && pin.mode == mode;
        });

        return found != PinTable.end() ? &*found : nullptr;
    }

    Str formatHash(u64 hash) {
        return std::format("{:#018x}", hash);
    }

    Str formatPin(const PuzzlePin& pin) {
        auto text = std::format(
            "    PuzzlePin{{PuzzleKind::{}, PuzzleMode::{}, {}, {{\n",
            enumName(pin.kind),
            enumName(pin.mode),
            pin.solvedAt
        );
        for(usize i = 0; i < pin.hashes.size(); ++i) {
            text += i % 3 == 0 ? "        " : " ";
            text += formatHash(pin.hashes[i]) + ",";
            if(i % 3 == 2)
                text += "\n";
        }
        if(pin.hashes.size() % 3 != 0)
            text += "\n";
        text += "    }},\n";

        return text;
    }

    Str firstDivergence(const PuzzlePin& pinned, const PuzzlePin& actual) {
        for(usize i = 0; i < PinTicks.size(); ++i) {
            if(pinned.hashes[i] != actual.hashes[i]) {
                return std::format(
                    "tick {}: pinned {}, actual {}",
                    PinTicks[i],
                    formatHash(pinned.hashes[i]),
                    formatHash(actual.hashes[i])
                );
            }
        }
        if(pinned.solvedAt != actual.solvedAt) {
            return std::format(
                "solvedAt: pinned {}, actual {}",
                pinned.solvedAt,
                actual.solvedAt
            );
        }

        return {};
    }
}
