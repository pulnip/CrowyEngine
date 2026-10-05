#pragma once

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
    struct PoseKey;
    struct PoseTrack;
    struct HingeRelease;
    struct PuzzleScript;

    using PuzzleBodies = std::vector<PuzzleBody>;
    using PuzzleHinges = std::vector<PuzzleHinge>;
    using PuzzleZones = std::vector<PuzzleZone>;
    using PoseKeys = std::vector<PoseKey>;
    using PoseTracks = std::vector<PoseTrack>;
    using HingeReleases = std::vector<HingeRelease>;
    using Waters = std::vector<WaterDesc>;

    inline constexpr u64 PuzzleHorizon = 600;
    // every body but the floor, whose top is y = 0, stays within
    // |x|, |z| <= half width and y in [0, height] of its own origin
    inline constexpr f32 PuzzleHalfWidth = 1.5f;
    inline constexpr f32 PuzzleHeight = 3.5f;

    // tall ones last: a viewer lays them out in rows of three
    enum class PuzzleKind : u8 {
        KickerJump,
        TiltTray,
        CarriedScoop,
        SwingCut,
        LeverCatapult,
        BuoyPop,
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
        // kinematic, moved by a track
        Mover,
        // turns to pass the motion on
        Lever,
        Weight,
        // what the goal asks for
        Payload,
        // where the payload ends up; lit once solved
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

    // both modes build the same bodies in the same order; scripts differ
    Puzzle makePuzzle(PuzzleKind kind);
    const PuzzleScript& scriptOf(const Puzzle& puzzle, PuzzleMode mode);
    // where a track has a body after the Step that ends at `tick`
    BodyPose poseAt(const PoseTrack& track, const BodyPose& start, u64 tick);
    // the releases a script applies by its last tick
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
        // drawn as a rod from the pivot to the body while it holds
        bool tethered = false;
    };

    // a box the puzzle reads with Overlapping; not a body
    struct PuzzleZone {
        std::string_view name;
        Vec3 center = zeros();
        Vec3 halfExtent{0.5f, 0.5f, 0.5f};
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
        PoseTracks tracks;
        HingeReleases releases;
    };

    // solved once the body is inside the zone
    struct PuzzleGoal {
        u32 body = 0;
        u32 zone = 0;
    };

    struct Puzzle {
        PuzzleKind kind = PuzzleKind::KickerJump;
        // what the solution does, in a few words
        std::string_view task;
        PuzzleBodies bodies;
        PuzzleHinges hinges;
        PuzzleZones zones;
        Waters waters;
        PuzzleScript solution;
        PuzzleScript control;
        PuzzleGoal goal;
    };

    inline constexpr usize eventCountOf(const PuzzleScript& script) {
        return script.releases.size();
    }

    CROWY_ENUM_BEGIN(PuzzleKind)
    CROWY_ENUM_VALUE(KickerJump)
    CROWY_ENUM_VALUE(TiltTray)
    CROWY_ENUM_VALUE(CarriedScoop)
    CROWY_ENUM_VALUE(SwingCut)
    CROWY_ENUM_VALUE(LeverCatapult)
    CROWY_ENUM_VALUE(BuoyPop)
    CROWY_ENUM_END()

    CROWY_ENUM_BEGIN(PuzzleMode)
    CROWY_ENUM_VALUE(Solution)
    CROWY_ENUM_VALUE(Control)
    CROWY_ENUM_END()

    CROWY_ENUM_BEGIN(PieceRole)
    CROWY_ENUM_VALUE(Ground)
    CROWY_ENUM_VALUE(Wall)
    CROWY_ENUM_VALUE(Mover)
    CROWY_ENUM_VALUE(Lever)
    CROWY_ENUM_VALUE(Weight)
    CROWY_ENUM_VALUE(Payload)
    CROWY_ENUM_VALUE(Goal)
    CROWY_ENUM_END()

    CROWY_ENUM_BEGIN(Easing)
    CROWY_ENUM_VALUE(Linear)
    CROWY_ENUM_VALUE(Smooth)
    CROWY_ENUM_VALUE(EaseIn)
    CROWY_ENUM_VALUE(EaseOut)
    CROWY_ENUM_END()
}
