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
    constexpr std::array<PuzzlePin, 10> PinTable{{
    PuzzlePin{PuzzleKind::KickerJump, PuzzleMode::Solution, 146, {
        0xa27c2c571e71a67e, 0xf718204c9d2a5e31, 0x751d26d3cf464827,
        0x05ba0e444db97a33, 0x49165b3ceda88174, 0x9637320b43e2dbc2,
    }},
    PuzzlePin{PuzzleKind::KickerJump, PuzzleMode::Control, 0, {
        0xa27c2c571e71a67e, 0xf718204c9d2a5e31, 0x751d26d3cf464827,
        0x136b064af9a53116, 0x0c4d981533a5f4c9, 0x1a7199d49afafd53,
    }},
    PuzzlePin{PuzzleKind::TiltTray, PuzzleMode::Solution, 423, {
        0xeb46ab15accd4790, 0xe564072a33281be5, 0x226877fd42e93166,
        0xead776851d73a58d, 0x8b2178ee1f6e3935, 0xaeb2aa21122dceb7,
    }},
    PuzzlePin{PuzzleKind::TiltTray, PuzzleMode::Control, 0, {
        0xeb46ab15accd4790, 0xf83609b6769c49bf, 0x355e38cd62cc215e,
        0x8291f705374cc8d3, 0x6dd2d383698c0fd6, 0xe0d308cd11c11a59,
    }},
    PuzzlePin{PuzzleKind::CarriedScoop, PuzzleMode::Solution, 309, {
        0x21108206bb49950c, 0xb15f6747f1a22fcc, 0x806068e53576aaff,
        0x2cd682d0bb8ba55b, 0xcb67ade9330abaf4, 0x9741e685a33f8b3f,
    }},
    PuzzlePin{PuzzleKind::CarriedScoop, PuzzleMode::Control, 0, {
        0x21108206bb49950c, 0xb15f6747f1a22fcc, 0x4f7da524166713c6,
        0x4aa2816557771b2b, 0xa7abc7d7f12d038b, 0x291337b3c243c629,
    }},
    PuzzlePin{PuzzleKind::SwingCut, PuzzleMode::Solution, 77, {
        0x2bf0a4375d246b1c, 0x02a28a66c96b6f10, 0x08e941ba9cee7761,
        0x2ce13cd8fcce2414, 0x942d9115cb21a837, 0x40de5201cca89c59,
    }},
    PuzzlePin{PuzzleKind::SwingCut, PuzzleMode::Control, 0, {
        0x2bf0a4375d246b1c, 0x02a28a66c96b6f10, 0x08e941ba9cee7761,
        0xd8630d2b951f901b, 0x92e38a51e11a569d, 0x8a1ad0e78f35ae90,
    }},
    PuzzlePin{PuzzleKind::BuoyPop, PuzzleMode::Solution, 117, {
        0xa0d73d7fc46af87e, 0xb0222a2896d776e2, 0xd102322e2785613c,
        0xb65c85207ffa75bc, 0xa47e3324f23c99fb, 0x1ebb0fc1d306e409,
    }},
    PuzzlePin{PuzzleKind::BuoyPop, PuzzleMode::Control, 0, {
        0xa0d73d7fc46af87e, 0xbdaf490eab4df7e1, 0xb41208a13022d059,
        0xe32fd37b35fa6f14, 0x8fd5182c54da7d5c, 0x5f6db3bd7dae85cd,
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
