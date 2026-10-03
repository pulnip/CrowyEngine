#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include "EnumUtil.hpp"
#include "PhysicsTypes.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    struct Puzzle;
    struct PuzzleBody;
    struct PuzzleHinge;
    struct PuzzleZone;
    struct PuzzleInput;
    struct PlateRule;

    using PuzzleBodies = std::vector<PuzzleBody>;
    using PuzzleHinges = std::vector<PuzzleHinge>;
    using PuzzleZones = std::vector<PuzzleZone>;
    using PuzzleInputs = std::vector<PuzzleInput>;
    using PlateRules = std::vector<PlateRule>;

    inline constexpr u64 PuzzleHorizon = 600;
    // every body but the floor, whose top is y = 0, stays within
    // |x|, |z| <= half width and y in [0, height] of its own origin
    inline constexpr f32 PuzzleHalfWidth = 1.5f;
    inline constexpr f32 PuzzleHeight = 3.5f;

    enum class PuzzleKind : u8 {
        PlateGate,
        ToppleBridge,
        SwingDoor,
    };

    enum class PuzzleMode : u8 {
        // the scripted solution
        Solution,
        // the solution without the move that works the mechanism
        Control,
    };

    // what a body is for, so a viewer can pick its look
    enum class PieceRole : u8 {
        Ground,
        Wall,
        Gate,
        Crate,
        Slab,
        Door,
        Ball,
    };

    enum class ZoneRole : u8 {
        Plate,
        Goal,
    };

    // both modes build the same bodies in the same order; inputs differ
    Puzzle makePuzzle(PuzzleKind kind);
    // the level a plate rule moves to from `level` with `mass` on it
    inline constexpr u32 nextPlateLevel(
        u32 level,
        f32 mass,
        const PlateRule& rule
    );
    const PuzzleInputs& inputsOf(const Puzzle& puzzle, PuzzleMode mode);

    struct PuzzleBody {
        std::string_view name;
        PieceRole role = PieceRole::Wall;
        BodyDesc desc;
    };

    // desc.body is filled from `body` when a session builds
    struct PuzzleHinge {
        u32 body = 0;
        HingeDesc desc;
    };

    // a box the puzzle reads with Overlapping; not a body
    struct PuzzleZone {
        std::string_view name;
        ZoneRole role = ZoneRole::Goal;
        Vec3 center = zeros();
        Vec3 halfExtent{0.5f, 0.5f, 0.5f};
    };

    // applied when the world's tick is `tick`, before it steps; no point
    // means the center of mass
    struct PuzzleInput {
        u64 tick = 0;
        u32 body = 0;
        Vec3 impulse = zeros();
        std::optional<Vec3> point;
    };

    // while the dynamic mass in `zone` reaches minMass the gate climbs one
    // level a tick, else sinks one, within [0, levels]
    struct PlateRule {
        u32 zone = 0;
        f32 minMass = 0.0f;
        u32 gate = 0;
        Vec3 rise = zeros();
        u32 levels = 0;
    };

    // solved once the body is inside the zone
    struct PuzzleGoal {
        u32 body = 0;
        u32 zone = 0;
    };

    struct Puzzle {
        PuzzleKind kind = PuzzleKind::PlateGate;
        PuzzleBodies bodies;
        PuzzleHinges hinges;
        PuzzleZones zones;
        PlateRules plates;
        PuzzleInputs solution;
        PuzzleInputs control;
        PuzzleGoal goal;
    };

    inline constexpr u32 nextPlateLevel(
        u32 level,
        f32 mass,
        const PlateRule& rule
    ) {
        if(mass >= rule.minMass)
            return level < rule.levels ? level + 1 : rule.levels;

        return level > 0 ? level - 1 : 0;
    }

    CROWY_ENUM_BEGIN(PuzzleKind)
    CROWY_ENUM_VALUE(PlateGate)
    CROWY_ENUM_VALUE(ToppleBridge)
    CROWY_ENUM_VALUE(SwingDoor)
    CROWY_ENUM_END()

    CROWY_ENUM_BEGIN(PuzzleMode)
    CROWY_ENUM_VALUE(Solution)
    CROWY_ENUM_VALUE(Control)
    CROWY_ENUM_END()
}
