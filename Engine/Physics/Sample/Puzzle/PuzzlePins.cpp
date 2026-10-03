#include "PuzzlePins.hpp"

#include <algorithm>
#include <format>

namespace
{
    using Crowy::PuzzleKind;
    using Crowy::PuzzleMode;
    using Crowy::PuzzlePin;

    // pasted from PuzzlePins.MatchTheTable's output on Windows, never by hand
    // clang-format off
    constexpr std::array<PuzzlePin, 6> PinTable{{
    PuzzlePin{PuzzleKind::PlateGate, PuzzleMode::Solution, 187, {
        0x636bbcfa79c8320c, 0xc14b62a7e6bfd252, 0x63154beb852170cd,
        0xf69a04f61dd4e4f1, 0xb8e9ef2eb1071c65, 0x8ac99059ce2b6ff2,
    }},
    PuzzlePin{PuzzleKind::PlateGate, PuzzleMode::Control, 0, {
        0x636bbcfa79c8320c, 0x55f52e30d1de89a8, 0x41b4f202046fa82d,
        0xde66b174312f2291, 0xf3d20922af1bce29, 0x13aa45d2a89dc113,
    }},
    PuzzlePin{PuzzleKind::ToppleBridge, PuzzleMode::Solution, 218, {
        0x4cef40ec21c3429a, 0x94f8110f98bb4b15, 0xc4d107976cc0398f,
        0xf56158fd11ff6b19, 0x51602ef19153be67, 0x711c3a77fb09d646,
    }},
    PuzzlePin{PuzzleKind::ToppleBridge, PuzzleMode::Control, 0, {
        0x4cef40ec21c3429a, 0x82d20aa7510d28fb, 0xd8b4b0a78ce125b5,
        0x2df64c357ad85ef1, 0x879c6599a937ffc1, 0x6859b29ef23651b7,
    }},
    PuzzlePin{PuzzleKind::SwingDoor, PuzzleMode::Solution, 197, {
        0x4d58f3111515a56e, 0xafdeccaef73a3a76, 0xcfa3a8972511bb48,
        0x498dfb2c50345b46, 0x60d0319867828068, 0x9144101e184ce7c9,
    }},
    PuzzlePin{PuzzleKind::SwingDoor, PuzzleMode::Control, 0, {
        0x4d58f3111515a56e, 0xdcf8ba9bdfbec116, 0xb9c0f2a9fab04094,
        0x4b9ad11a06c9b6a5, 0x081e79d0c184481f, 0xfacf912158cb21e1,
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
