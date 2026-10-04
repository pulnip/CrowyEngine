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
    constexpr std::array<PuzzlePin, 12> PinTable{{
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
        0x6ef137df2d453469, 0x8b83a524c43335a8, 0x087dd04b40947c32,
    }},
    PuzzlePin{PuzzleKind::TiltTray, PuzzleMode::Control, 0, {
        0xeb46ab15accd4790, 0xf83609b6769c49bf, 0x355e38cd62cc215e,
        0x8291f705374cc8d3, 0xbf99189149babd38, 0x8df87bdd368936e2,
    }},
    PuzzlePin{PuzzleKind::CarriedScoop, PuzzleMode::Solution, 309, {
        0x21108206bb49950c, 0x9c0efd11d1db6e6f, 0x28cc75c6812f6b61,
        0xac5d898a4c3c2854, 0xe0b833d2d45649e5, 0x7c6974ecce58458b,
    }},
    PuzzlePin{PuzzleKind::CarriedScoop, PuzzleMode::Control, 0, {
        0x21108206bb49950c, 0x9c0efd11d1db6e6f, 0x28cc75c6812f6b61,
        0x2c07e8cd026ecb5e, 0x4f04e97a0df3a12d, 0xf244c4aeda3f85da,
    }},
    PuzzlePin{PuzzleKind::SwingCut, PuzzleMode::Solution, 77, {
        0x2bf0a4375d246b1c, 0x02a28a66c96b6f10, 0x08e941ba9cee7761,
        0x2ce13cd8fcce2414, 0x942d9115cb21a837, 0x40de5201cca89c59,
    }},
    PuzzlePin{PuzzleKind::SwingCut, PuzzleMode::Control, 0, {
        0x2bf0a4375d246b1c, 0x02a28a66c96b6f10, 0x08e941ba9cee7761,
        0xd8630d2b951f901b, 0x92e38a51e11a569d, 0x8a1ad0e78f35ae90,
    }},
    PuzzlePin{PuzzleKind::LeverCatapult, PuzzleMode::Solution, 202, {
        0xc1dfad1a22383d9a, 0xf3fa5505f9ea8d62, 0xec2ffed79ecfa784,
        0xc063e5bfd3ea236b, 0x4f340ac9b745c140, 0x85d4fcba3dd94692,
    }},
    PuzzlePin{PuzzleKind::LeverCatapult, PuzzleMode::Control, 0, {
        0xc1dfad1a22383d9a, 0xd1461bc940ac708e, 0x3d99f0b191f9dacd,
        0x1743834c2cd8209b, 0x79a70cecbb428a7b, 0xaea23972fdac721b,
    }},
    PuzzlePin{PuzzleKind::BuoyPop, PuzzleMode::Solution, 117, {
        0xa0d73d7fc46af87e, 0xb0222a2896d776e2, 0xd102322e2785613c,
        0xd8ad5d9f21f53e86, 0xa47e3324f23c99fb, 0x0ea53eb8c01899e5,
    }},
    PuzzlePin{PuzzleKind::BuoyPop, PuzzleMode::Control, 0, {
        0xa0d73d7fc46af87e, 0xbdaf490eab4df7e1, 0x7cd104b4efd54034,
        0xd5d8077b7d75e227, 0xe8792eca01f00c32, 0x10648676d27eb51b,
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
