#include "PuzzleSession.hpp"

#include <algorithm>
#include <ranges>

#include "Assert.hpp"

namespace Crowy
{
    PuzzleSession::PuzzleSession(
        PhysicsRuntime& runtime,
        PuzzleKind kind,
        PuzzleMode mode
    )
        : world(std::make_unique<PhysicsWorld>(runtime)),
          puzzle(makePuzzle(kind)),
          mode(mode),
          plateLevels(puzzle.plates.size(), 0),
          pin(findPin(kind, mode)),
          actual{.kind = kind, .mode = mode} {
        const auto& inputs = inputsOf(puzzle, this->mode);
        CROWY_ASSERT(
            std::ranges::is_sorted(inputs, {}, &PuzzleInput::tick),
            "a puzzle's inputs run in tick order"
        );

        bodies.reserve(puzzle.bodies.size());
        for(const auto& body: puzzle.bodies)
            bodies.push_back(world->CreateBody(body.desc));
        for(const auto& hinge: puzzle.hinges) {
            auto desc = hinge.desc;
            desc.body = HandleOf(hinge.body);
            hinges.push_back(world->CreateHinge(desc));
        }

        status.hash = world->StateHash();
        checkpoint();
    }

    PuzzleSession::~PuzzleSession() = default;

    void PuzzleSession::Tick() {
        applyDueInputs();
        applyPlateRules();
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

    u32 PuzzleSession::PlateLevelOf(u32 plate) const {
        CROWY_ASSERT(
            plate < plateLevels.size(),
            "the puzzle has no plate {}",
            plate
        );

        return plateLevels[plate];
    }

    void PuzzleSession::applyDueInputs() {
        const auto& inputs = inputsOf(puzzle, mode);
        const auto tick = world->TickCount();
        for(; nextInput < inputs.size() && inputs[nextInput].tick == tick;
            ++nextInput) {
            const auto& input = inputs[nextInput];
            if(input.point)
                world->AddImpulseAt(
                    HandleOf(input.body),
                    input.impulse,
                    *input.point
                );
            else
                world->AddImpulse(HandleOf(input.body), input.impulse);
        }
    }

    // reads the state the last Step left, so a tick's rule never sees
    // its own Step
    void PuzzleSession::applyPlateRules() {
        for(usize i = 0; i < puzzle.plates.size(); ++i) {
            const auto& rule = puzzle.plates[i];
            const auto& zone = puzzle.zones[rule.zone];

            const auto onPlate =
                world->Overlapping(zone.center, zone.halfExtent);
            f32 mass = 0.0f;
            for(const auto body: onPlate) {
                if(world->MotionOf(body) == BodyMotion::Dynamic)
                    mass += world->MassOf(body);
            }

            auto& level = plateLevels[i];
            const auto previous = level;
            if(mass >= rule.minMass)
                level = std::min(level + 1, rule.levels);
            else if(level > 0)
                --level;
            if(level == previous)
                continue;

            const auto& gate = puzzle.bodies[rule.gate].desc.pose;
            world->MoveKinematic(
                HandleOf(rule.gate),
                BodyPose{
                    .position = gate.position +
                        rule.rise * static_cast<f32>(level),
                    .rotation = gate.rotation,
                }
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
