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
        RAII<Impl> impl;

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

        void AddImpulse(BodyHandle body, Vec3 impulse);
        void AddImpulseAt(BodyHandle body, Vec3 impulse, Vec3 worldPoint);
        void MoveKinematic(BodyHandle body, const BodyPose& target);
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
        f32 MassOf(BodyHandle body) const;
        bool IsAwake(BodyHandle body) const;
        f32 HingeAngleOf(HingeHandle hinge) const;
        bool IsHingeHeld(HingeHandle hinge) const;

        BodyHandles Overlapping(Vec3 center, Vec3 halfExtent) const;
        u64 StateHash() const;
    };
}
