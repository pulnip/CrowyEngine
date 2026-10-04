#include "PuzzleSession.hpp"

#include <algorithm>
#include <ranges>
#include <utility>

#include "Assert.hpp"

namespace Crowy
{
    PuzzleSession::PuzzleSession(
        PhysicsRuntime& runtime,
        PuzzleKind kind,
        PuzzleMode mode
    )
        : PuzzleSession(runtime, makePuzzle(kind), mode) {}

    PuzzleSession::PuzzleSession(
        PhysicsRuntime& runtime,
        Puzzle puzzle,
        PuzzleMode mode
    )
        : world(std::make_unique<PhysicsWorld>(runtime)),
          puzzle(std::move(puzzle)),
          mode(mode),
          pin(findPin(this->puzzle.kind, mode)),
          actual{.kind = this->puzzle.kind, .mode = mode} {
        const auto& content = this->puzzle;
        const auto& script = scriptOf(content, this->mode);
        CROWY_ASSERT(
            std::ranges::is_sorted(script.releases, {}, &HingeRelease::tick),
            "a puzzle's releases run in tick order"
        );
        for(const auto& track: script.tracks) {
            CROWY_ASSERT(
                content.bodies[track.body].desc.motion == BodyMotion::Kinematic,
                "a track moves a Kinematic body"
            );
        }

        bodies.reserve(content.bodies.size());
        for(const auto& body: content.bodies)
            bodies.push_back(world->CreateBody(body.desc));
        for(const auto& hinge: content.hinges) {
            auto desc = hinge.desc;
            desc.body = HandleOf(hinge.body);
            hinges.push_back(world->CreateHinge(desc));
        }
        for(const auto& water: content.waters)
            world->AddWater(water);

        status.hash = world->StateHash();
        checkpoint();
    }

    PuzzleSession::~PuzzleSession() = default;

    void PuzzleSession::Tick() {
        applyReleases();
        driveTracks();
        world->Step();
        status.tick = world->TickCount();
        latchGoal();
        status.hash = world->StateHash();
        checkpoint();
    }

    BodyHandle PuzzleSession::HandleOf(u32 body) const {
        CROWY_ASSERT(body < bodies.size(), "the puzzle has no body {}", body);

        return bodies[body];
    }

    HingeHandle PuzzleSession::HingeHandleOf(u32 hinge) const {
        CROWY_ASSERT(
            hinge < hinges.size(),
            "the puzzle has no hinge {}",
            hinge
        );

        return hinges[hinge];
    }

    void PuzzleSession::applyReleases() {
        const auto& releases = scriptOf(puzzle, mode).releases;
        const auto tick = world->TickCount();
        for(; nextRelease < releases.size() &&
              releases[nextRelease].tick == tick;
            ++nextRelease) {
            world->ReleaseHinge(HingeHandleOf(releases[nextRelease].hinge));
            ++status.eventsApplied;
        }
    }

    // every track every tick, toward where its key puts it after this Step
    void PuzzleSession::driveTracks() {
        const auto next = world->TickCount() + 1;
        for(const auto& track: scriptOf(puzzle, mode).tracks) {
            world->MoveKinematic(
                HandleOf(track.body),
                poseAt(track, puzzle.bodies[track.body].desc.pose, next)
            );
        }
    }

    void PuzzleSession::latchGoal() {
        if(status.solved)
            return;

        const auto& zone = puzzle.zones[puzzle.goal.zone];
        const auto inside = world->Overlapping(zone.center, zone.halfExtent);
        if(std::ranges::contains(inside, HandleOf(puzzle.goal.body))) {
            status.solved = true;
            status.solvedAt = status.tick;
        }
    }

    void PuzzleSession::checkpoint() {
        const auto found = std::ranges::find(PinTicks, status.tick);
        if(found == PinTicks.end())
            return;

        const auto index = static_cast<usize>(found - PinTicks.begin());
        actual.hashes[index] = status.hash;
        if(pin != nullptr) {
            if(pin->hashes[index] == status.hash)
                ++status.checkpointsMatched;
            else
                ++status.checkpointsMissed;
        }

        if(status.tick != PuzzleHorizon)
            return;
        actual.solvedAt = status.solvedAt;
        if(pin != nullptr) {
            if(pin->solvedAt == status.solvedAt)
                ++status.checkpointsMatched;
            else
                ++status.checkpointsMissed;
        }
    }
}
