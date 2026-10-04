#pragma once

#include <memory>

#include "PhysicsRuntime.hpp"
#include "PhysicsTypes.hpp"
#include "Primitives.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    struct PhysicsWorldDesc {
        Vec3 gravity{0.0f, -9.81f, 0.0f};
        u32 maxBodies = 1024;
        u32 maxBodyPairs = 1024;
        u32 maxContactConstraints = 1024;
    };

    // Bodies on a fixed tick. Called from one thread, never during a Step;
    // the worlds of one runtime step one at a time.
    class PhysicsWorld {
    private:
        class Impl;

    private:
        std::unique_ptr<Impl> impl;

    public:
        ~PhysicsWorld();
        CROWY_DECLARE_PINNED(PhysicsWorld)

        explicit PhysicsWorld(
            PhysicsRuntime& runtime,
            const PhysicsWorldDesc& desc = {}
        );

        // a full world asserts; without asserts it logs and returns invalid
        BodyHandle CreateBody(const BodyDesc& desc);
        HingeHandle CreateHinge(const HingeDesc& desc);

        // dynamic bodies only; wakes the body
        void AddImpulse(BodyHandle body, Vec3 impulse);
        void AddImpulseAt(BodyHandle body, Vec3 impulse, Vec3 worldPoint);
        // kinematic bodies only; held, and reissued before every Step
        void MoveKinematic(BodyHandle body, const BodyPose& target);
        // one way: the body swings free from the next Step, and wakes
        void ReleaseHinge(HingeHandle hinge);
        // applied before every Step from then on
        void AddWater(const WaterDesc& water);

        // one tick of PhysicsTickSeconds
        void Step();

        bool IsValid(BodyHandle body) const noexcept;
        bool IsValid(HingeHandle hinge) const noexcept;
        u64 TickCount() const noexcept;
        usize BodyCount() const noexcept;

        BodyMotion MotionOf(BodyHandle body) const;
        BodyShape ShapeOf(BodyHandle body) const;
        BodyPose PoseOf(BodyHandle body) const;
        Vec3 LinearVelocityOf(BodyHandle body) const;
        // dynamic bodies only: the mass the body was created with
        f32 MassOf(BodyHandle body) const;
        bool IsAwake(BodyHandle body) const;
        f32 HingeAngleOf(HingeHandle hinge) const;
        bool IsHingeHeld(HingeHandle hinge) const;

        // non-static bodies overlapping the box, sleepers included, in
        // creation order
        BodyHandles Overlapping(Vec3 center, Vec3 halfExtent) const;
        // the tick and every byte of simulation state, so two worlds driven
        // alike hash alike
        u64 StateHash() const;
    };
}
