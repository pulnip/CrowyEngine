#include <algorithm>
#include <format>
#include <print>
#include <vector>

#include <gtest/gtest.h>

#include "PhysicsRuntime.hpp"
#include "PuzzlePins.hpp"
#include "PuzzleRun.hpp"
#include "PuzzleSession.hpp"

using namespace Crowy;

namespace
{
    // the table's whole body, ready to paste over PuzzlePins.cpp's
    Str formatTable(const std::vector<PuzzlePin>& pins) {
        auto text = std::format(
            "    constexpr std::array<PuzzlePin, {}> PinTable{{{{\n",
            pins.size()
        );
        for(const auto& pin: pins)
            text += formatPin(pin);
        text += "    }};\n";

        return text;
    }
}

TEST(PuzzlePins, TableCoversEveryRunOnce) {
    const auto pins = puzzlePins();
    EXPECT_EQ(pins.size(), AllPuzzleKinds.size() * AllPuzzleModes.size());
    for(const auto kind: AllPuzzleKinds) {
        for(const auto mode: AllPuzzleModes) {
            auto isThisRun = [&](const PuzzlePin& pin) {
                return pin.kind == kind && pin.mode == mode;
            };
            const auto count = std::ranges::count_if(pins, isThisRun);
            EXPECT_EQ(count, 1) << enumName(kind) << " " << enumName(mode);
            EXPECT_NE(findPin(kind, mode), nullptr);
        }
    }
}

// Windows records the table; elsewhere a mismatch is drift to report
TEST(PuzzlePins, MatchTheTable) {
    PhysicsRuntime runtime;
    std::vector<PuzzlePin> actual;
    u32 matched = 0;
    for(const auto kind: AllPuzzleKinds) {
        for(const auto mode: AllPuzzleModes) {
            PuzzleSession session(runtime, kind, mode);
            runTo(session, PuzzleHorizon);
            actual.push_back(session.GetActualPin());

            const auto* pinned = session.GetPin();
            if(pinned == nullptr) {
                ADD_FAILURE() << "[" << enumName(kind) << " "
                              << enumName(mode) << "] unpinned";
                continue;
            }
            const auto divergence =
                firstDivergence(*pinned, session.GetActualPin());
            EXPECT_TRUE(divergence.empty())
                << "[" << enumName(kind) << " " << enumName(mode) << "] "
                << divergence;
            EXPECT_EQ(session.GetStatus().checkpointsMissed, 0u);
            matched += session.GetStatus().checkpointsMatched;
        }
    }
    // six hashes and solvedAt per run
    EXPECT_EQ(matched, 6u * (PinTicks.size() + 1));
    if(HasFailure())
        std::println("this binary's table:\n{}", formatTable(actual));
}

TEST(PuzzlePins, PrintsTableSyntax) {
    EXPECT_EQ(formatHash(0xabc), "0x0000000000000abc");

    const auto pin = PuzzlePin{
        .kind = PuzzleKind::ToppleBridge,
        .mode = PuzzleMode::Control,
        .solvedAt = 0,
        .hashes = {1, 2, 3, 4, 5, 0xffff'ffff'ffff'ffff},
    };
    EXPECT_EQ(
        formatPin(pin),
        "    PuzzlePin{PuzzleKind::ToppleBridge, PuzzleMode::Control, 0, {\n"
        "        0x0000000000000001, 0x0000000000000002, 0x0000000000000003,\n"
        "        0x0000000000000004, 0x0000000000000005, 0xffffffffffffffff,\n"
        "    }},\n"
    );

    auto later = pin;
    EXPECT_EQ(firstDivergence(pin, later), "");
    later.solvedAt = 300;
    EXPECT_EQ(firstDivergence(pin, later), "solvedAt: pinned 0, actual 300");
    later.hashes[0] = 9;
    EXPECT_EQ(
        firstDivergence(pin, later),
        "tick 0: pinned 0x0000000000000001, actual 0x0000000000000009"
    );
}
