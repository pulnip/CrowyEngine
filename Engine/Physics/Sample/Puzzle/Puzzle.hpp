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
    struct PoseKey;
    struct PoseTrack;
    struct HingeRelease;
    struct PuzzleScript;

    using PuzzleBodies = std::vector<PuzzleBody>;
    using PuzzleHinges = std::vector<PuzzleHinge>;
    using PuzzleZones = std::vector<PuzzleZone>;
    using PuzzleInputs = std::vector<PuzzleInput>;
    using PlateRules = std::vector<PlateRule>;
    using PoseKeys = std::vector<PoseKey>;
    using PoseTracks = std::vector<PoseTrack>;
    using HingeReleases = std::vector<HingeRelease>;
    using Waters = std::vector<WaterDesc>;

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

    // how a pose track moves from one key to the next
    enum class Easing : u8 {
        Linear,
        // starts and ends at rest
        Smooth,
        // starts at rest and ends moving
        EaseIn,
        // starts moving and ends at rest
        EaseOut,
    };

    // both modes build the same bodies in the same order; inputs differ
    Puzzle makePuzzle(PuzzleKind kind);
    // the level a plate rule moves to from `level` with `mass` on it
    inline constexpr u32 nextPlateLevel(
        u32 level,
        f32 mass,
        const PlateRule& rule
    );
    const PuzzleScript& scriptOf(const Puzzle& puzzle, PuzzleMode mode);
    // where a track has a body after the Step that ends at `tick`
    BodyPose poseAt(const PoseTrack& track, const BodyPose& start, u64 tick);
    // the impulses and releases a script applies by its last tick
    inline constexpr usize eventCountOf(const PuzzleScript& script);

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

    // the pose reached after the Step that ends at `tick`
    struct PoseKey {
        u64 tick = 0;
        BodyPose pose;
        Easing easing = Easing::Smooth;
    };

    // a kinematic body from its desc pose at tick 0 through rising keys,
    // held after the last
    struct PoseTrack {
        u32 body = 0;
        PoseKeys keys;
    };

    // the hinge lets go when the world's tick is `tick`, before it steps
    struct HingeRelease {
        u64 tick = 0;
        u32 hinge = 0;
    };

    // what the player does in one mode
    struct PuzzleScript {
        PuzzleInputs impulses;
        PoseTracks tracks;
        HingeReleases releases;
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
        Waters waters;
        PlateRules plates;
        PuzzleScript solution;
        PuzzleScript control;
        PuzzleGoal goal;
    };

    inline constexpr usize eventCountOf(const PuzzleScript& script) {
        return script.impulses.size() + script.releases.size();
    }

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

    CROWY_ENUM_BEGIN(Easing)
    CROWY_ENUM_VALUE(Linear)
    CROWY_ENUM_VALUE(Smooth)
    CROWY_ENUM_VALUE(EaseIn)
    CROWY_ENUM_VALUE(EaseOut)
    CROWY_ENUM_END()
}
