#pragma once

#include <memory>
#include <vector>

#include "PhysicsRuntime.hpp"
#include "PhysicsWorld.hpp"
#include "Primitives.hpp"
#include "Puzzle.hpp"
#include "PuzzlePins.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    using HingeHandles = std::vector<HingeHandle>;

    struct PuzzleStatus {
        u64 tick = 0;
        // StateHash after the last Tick
        u64 hash = 0;
        bool solved = false;
        // the first tick the goal held; 0 while unsolved
        u64 solvedAt = 0;
        // the pinned ticks reached so far, solvedAt counted at the horizon
        u32 checkpointsMatched = 0;
        u32 checkpointsMissed = 0;
        // the script's releases applied so far
        u32 eventsApplied = 0;

        friend bool operator==(const PuzzleStatus&, const PuzzleStatus&) =
            default;
    };

    // only Tick moves the world, so the tests and the playground run one
    // path; a restart is a new session
    class PuzzleSession {
    private:
        std::unique_ptr<PhysicsWorld> world;
        Puzzle puzzle;
        PuzzleMode mode = PuzzleMode::Solution;
        BodyHandles bodies;
        HingeHandles hinges;
        usize nextRelease = 0;
        // null when the table has no row for this run
        const PuzzlePin* pin = nullptr;
        PuzzlePin actual;
        PuzzleStatus status;

    public:
        ~PuzzleSession();
        CROWY_DECLARE_PINNED(PuzzleSession)

        PuzzleSession(
            PhysicsRuntime& runtime,
            PuzzleKind kind,
            PuzzleMode mode
        );
        // a variant of a table puzzle, as a sweep tries; the pins stay its
        // kind's
        PuzzleSession(PhysicsRuntime& runtime, Puzzle puzzle, PuzzleMode mode);

        // the due releases, the tracks, one Step, the goal, the checkpoint
        void Tick();

        const Puzzle& GetPuzzle() const noexcept { return puzzle; }
        PuzzleMode GetMode() const noexcept { return mode; }
        const PhysicsWorld& GetWorld() const noexcept { return *world; }
        const PuzzleStatus& GetStatus() const noexcept { return status; }
        const PuzzlePin* GetPin() const noexcept { return pin; }
        // this run's numbers at the pinned ticks reached so far
        const PuzzlePin& GetActualPin() const noexcept { return actual; }
        BodyHandle HandleOf(u32 body) const;
        HingeHandle HingeHandleOf(u32 hinge) const;

    private:
        void applyReleases();
        void driveTracks();
        void latchGoal();
        void checkpoint();
    };
}
