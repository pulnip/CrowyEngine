#include "PhysicsWorld.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <numbers>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <Jolt/Jolt.h>

#include <Jolt/Core/HashCombine.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/Collision/PhysicsMaterial.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/StateRecorderImpl.h>

#include "Assert.hpp"
#include "JoltConvert.hpp"
#include "LogLocal.hpp"
#include "PackedTable.hpp"
#include "PhysicsRuntimeImpl.hpp"
#include "VariantUtil.hpp"

namespace
{
    using Crowy::BodyMotion;
    using Crowy::BodyShape;
    using Crowy::f32;
    using Crowy::u32;
    using Crowy::u64;
    using HingeConstraintRef = JPH::Ref<JPH::HingeConstraint>;

    inline constexpr JPH::ObjectLayer StaticLayer = 0;
    inline constexpr JPH::ObjectLayer MovingLayer = 1;
    inline constexpr u32 ObjectLayerCount = 2;
    inline constexpr JPH::BroadPhaseLayer StaticBroadPhase{0};
    inline constexpr JPH::BroadPhaseLayer MovingBroadPhase{1};
    inline constexpr u32 BroadPhaseLayerCount = 2;
    // the table's own generation, below the world's tag
    inline constexpr u64 GenerationMask = 0xffff'ffffu;

    // never 0, so a default handle names no world
    std::atomic<u32> nextWorldTag{1};

    JPH::EMotionType joltMotion(BodyMotion motion) {
        using enum BodyMotion;

        switch(motion) {
        case Static:
            return JPH::EMotionType::Static;
        case Kinematic:
            return JPH::EMotionType::Kinematic;
        case Dynamic:
            return JPH::EMotionType::Dynamic;
        }
        return JPH::EMotionType::Static;
    }

    JPH::ObjectLayer layerOf(BodyMotion motion) {
        return motion == BodyMotion::Static ? StaticLayer : MovingLayer;
    }

    bool isUnit(Crowy::Vec4 rotation) {
        return Crowy::toJoltQuat(rotation).IsNormalized();
    }

    // A compound part's own friction; every other surface keeps sDefault.
    class PartMaterial final: public JPH::PhysicsMaterial {
    public:
        f32 friction = 0.0f;

    public:
        explicit PartMaterial(f32 friction)
            : friction(friction) {}
    };

    f32 frictionOf(const JPH::Body& body, const JPH::SubShapeID& part) {
        const auto* material = body.GetShape()->GetMaterial(part);
        if(material == JPH::PhysicsMaterial::sDefault.GetPtr())
            return body.GetFriction();

        return static_cast<const PartMaterial*>(material)->friction;
    }

    // Jolt's own rule, with a part's friction in place of its body's
    float combineFriction(
        const JPH::Body& body1,
        const JPH::SubShapeID& part1,
        const JPH::Body& body2,
        const JPH::SubShapeID& part2
    ) {
        return std::sqrt(frictionOf(body1, part1) * frictionOf(body2, part2));
    }

    JPH::ShapeSettings::ShapeResult makeBox(
        Crowy::Vec3 halfExtent,
        const JPH::PhysicsMaterial* material = nullptr
    ) {
        CROWY_ASSERT(
            halfExtent.x > 0.0f && halfExtent.y > 0.0f && halfExtent.z > 0.0f,
            "a box needs a positive half extent"
        );
        JPH::BoxShapeSettings settings(
            Crowy::toJolt(halfExtent),
            JPH::cDefaultConvexRadius,
            material
        );
        settings.SetEmbedded();

        return settings.Create();
    }

    JPH::ShapeRefC makeShape(const BodyShape& shape) {
        using ShapeResult = JPH::ShapeSettings::ShapeResult;

        const auto result = std::visit(
            Crowy::overload{
                [](const Crowy::BoxShape& box) -> ShapeResult {
                    return makeBox(box.halfExtent);
                },
                [](const Crowy::SphereShape& sphere) -> ShapeResult {
                    CROWY_ASSERT(
                        sphere.radius > 0.0f,
                        "a sphere needs a positive radius"
                    );
                    JPH::SphereShapeSettings settings(sphere.radius);
                    settings.SetEmbedded();
                    return settings.Create();
                },
                [](const Crowy::CompoundShape& compound) -> ShapeResult {
                    CROWY_ASSERT(
                        !compound.parts.empty(),
                        "a compound needs a part"
                    );
                    JPH::StaticCompoundShapeSettings settings;
                    settings.SetEmbedded();
                    for(const auto& part: compound.parts) {
                        CROWY_ASSERT(
                            isUnit(part.pose.rotation),
                            "a part needs a unit rotation"
                        );
                        CROWY_ASSERT(
                            part.friction.value_or(0.0f) >= 0.0f,
                            "a part's friction is not negative"
                        );
                        // the shape holds its material by reference count
                        const auto material = part.friction
                            ? JPH::RefConst<JPH::PhysicsMaterial>(
                                  new PartMaterial(*part.friction)
                              )
                            : nullptr;
                        const auto box =
                            makeBox(part.halfExtent, material.GetPtr());
                        CROWY_ASSERT(
                            !box.HasError(),
                            "{}",
                            box.GetError().c_str()
                        );
                        settings.AddShape(
                            Crowy::toJolt(part.pose.position),
                            Crowy::toJoltQuat(part.pose.rotation),
                            box.Get()
                        );
                    }
                    return settings.Create();
                },
            },
            shape
        );
        CROWY_ASSERT(!result.HasError(), "{}", result.GetError().c_str());

        return result.Get();
    }

    struct BodyRow {
        JPH::BodyID id;
        // creation order, which every result is sorted by; BodyIDs are reused
        u64 serial = 0;
        BodyMotion motion = BodyMotion::Dynamic;
        BodyShape shape;
        f32 mass = 0.0f;
        std::optional<Crowy::BodyPose> kinematicTarget;
    };

    struct HingeRow {
        HingeConstraintRef constraint;
    };
}

namespace Crowy
{
    class PhysicsWorld::Impl {
    public:
        using BodyRows = PackedTable<BodyRow>;
        using HingeRows = PackedTable<HingeRow>;
        using BodyRowHandles = std::vector<BodyRows::Handle>;
        using LayerVsBroadPhase =
            std::unique_ptr<JPH::ObjectVsBroadPhaseLayerFilterTable>;

    public:
        PhysicsRuntime::Impl& runtime;
        u32 tag = nextWorldTag.fetch_add(1);
        // the layer tables outlive the system, which keeps references
        JPH::ObjectLayerPairFilterTable layerPairs{ObjectLayerCount};
        JPH::BroadPhaseLayerInterfaceTable broadPhaseLayers{
            ObjectLayerCount,
            BroadPhaseLayerCount
        };
        // it copies the other two when built, so it is built after them
        LayerVsBroadPhase layerVsBroadPhase;
        JPH::PhysicsSystem system;
        BodyRows bodies;
        HingeRows hinges;
        // indexed by serial, which a body's user data holds
        BodyRowHandles rowBySerial;
        std::vector<WaterDesc> waters;
        u64 tickCount = 0;
        bool broadPhaseOptimized = false;

    public:
        ~Impl();
        CROWY_DECLARE_PINNED(Impl)

        Impl(PhysicsRuntime::Impl& runtime, const PhysicsWorldDesc& desc);

        JPH::BodyInterface& Bodies() noexcept {
            return system.GetBodyInterfaceNoLock();
        }

        BodyHandle PublicHandle(BodyRows::Handle handle) const noexcept;
        HingeHandle PublicHandle(HingeRows::Handle handle) const noexcept;
        std::optional<BodyRows::Handle> RowHandle(BodyHandle body) const;
        std::optional<HingeRows::Handle> RowHandle(HingeHandle hinge) const;

        BodyRow& Row(BodyHandle body);
        const HingeRow& Row(HingeHandle hinge) const;
    };

    PhysicsWorld::Impl::Impl(
        PhysicsRuntime::Impl& runtime,
        const PhysicsWorldDesc& desc
    )
        : runtime(runtime) {
        layerPairs.EnableCollision(MovingLayer, StaticLayer);
        layerPairs.EnableCollision(MovingLayer, MovingLayer);
        broadPhaseLayers.MapObjectToBroadPhaseLayer(
            StaticLayer,
            StaticBroadPhase
        );
        broadPhaseLayers.MapObjectToBroadPhaseLayer(
            MovingLayer,
            MovingBroadPhase
        );
        layerVsBroadPhase =
            std::make_unique<JPH::ObjectVsBroadPhaseLayerFilterTable>(
                broadPhaseLayers,
                BroadPhaseLayerCount,
                layerPairs,
                ObjectLayerCount
            );

        system.Init(
            desc.maxBodies,
            0,
            desc.maxBodyPairs,
            desc.maxContactConstraints,
            broadPhaseLayers,
            *layerVsBroadPhase,
            layerPairs
        );
        system.SetGravity(toJolt(desc.gravity));
        system.SetCombineFriction(combineFriction);
        ++this->runtime.worldCount;
    }

    PhysicsWorld::Impl::~Impl() {
        // constraints before their bodies, which do not know about them
        for(const auto& row: hinges.All())
            system.RemoveConstraint(row.constraint.GetPtr());
        hinges.Clear();

        auto& interface = Bodies();
        for(const auto handle: rowBySerial) {
            if(!bodies.IsValid(handle))
                continue;
            const auto id = bodies.GetRef(handle).id;
            interface.RemoveBody(id);
            interface.DestroyBody(id);
        }
        bodies.Clear();
        --runtime.worldCount;
    }

    BodyHandle PhysicsWorld::Impl::PublicHandle(
        BodyRows::Handle handle
    ) const noexcept {
        CROWY_ASSERT(handle.GetGeneration() <= GenerationMask);

        return BodyHandle{
            handle.GetIndex(),
            u64{tag} << 32 | handle.GetGeneration()
        };
    }

    HingeHandle PhysicsWorld::Impl::PublicHandle(
        HingeRows::Handle handle
    ) const noexcept {
        CROWY_ASSERT(handle.GetGeneration() <= GenerationMask);

        return HingeHandle{
            handle.GetIndex(),
            u64{tag} << 32 | handle.GetGeneration()
        };
    }

    std::optional<PhysicsWorld::Impl::BodyRows::Handle>
    PhysicsWorld::Impl::RowHandle(BodyHandle body) const {
        if(body.GetGeneration() >> 32 != tag)
            return std::nullopt;

        const auto handle = BodyRows::Handle{
            body.GetIndex(),
            body.GetGeneration() & GenerationMask
        };
        if(!bodies.IsValid(handle))
            return std::nullopt;

        return handle;
    }

    std::optional<PhysicsWorld::Impl::HingeRows::Handle>
    PhysicsWorld::Impl::RowHandle(HingeHandle hinge) const {
        if(hinge.GetGeneration() >> 32 != tag)
            return std::nullopt;

        const auto handle = HingeRows::Handle{
            hinge.GetIndex(),
            hinge.GetGeneration() & GenerationMask
        };
        if(!hinges.IsValid(handle))
            return std::nullopt;

        return handle;
    }

    BodyRow& PhysicsWorld::Impl::Row(BodyHandle body) {
        const auto handle = RowHandle(body);
        CROWY_ASSERT(
            handle.has_value(),
            "body ({}, {:#x}) is not a body of this world",
            body.GetIndex(),
            body.GetGeneration()
        );

        return bodies.GetRef(*handle);
    }

    const HingeRow& PhysicsWorld::Impl::Row(HingeHandle hinge) const {
        const auto handle = RowHandle(hinge);
        CROWY_ASSERT(
            handle.has_value(),
            "hinge ({}, {:#x}) is not a hinge of this world",
            hinge.GetIndex(),
            hinge.GetGeneration()
        );

        return hinges.GetRef(*handle);
    }

    PhysicsWorld::PhysicsWorld(
        PhysicsRuntime& runtime,
        const PhysicsWorldDesc& desc
    )
        : impl(std::make_unique<Impl>(*runtime.impl, desc)) {}

    PhysicsWorld::~PhysicsWorld() = default;

    BodyHandle PhysicsWorld::CreateBody(const BodyDesc& desc) {
        using enum BodyMotion;

        CROWY_ASSERT(
            isUnit(desc.pose.rotation),
            "a pose needs a unit rotation"
        );
        CROWY_ASSERT(desc.mass >= 0.0f, "a mass is never negative");

        JPH::BodyCreationSettings settings(
            makeShape(desc.shape),
            toJolt(desc.pose.position),
            toJoltQuat(desc.pose.rotation),
            joltMotion(desc.motion),
            layerOf(desc.motion)
        );
        settings.mFriction = desc.friction;
        settings.mRestitution = desc.restitution;
        settings.mLinearDamping = desc.linearDamping;
        settings.mAngularDamping = desc.angularDamping;
        // reduction merges coplanar hits into one manifold that keeps a single
        // part's ID, so a part's own friction would leak across a seam
        if(const auto* compound = std::get_if<CompoundShape>(&desc.shape)) {
            auto ownsFriction = [](const CompoundPart& part) {
                return part.friction.has_value();
            };
            settings.mUseManifoldReduction =
                std::ranges::none_of(compound->parts, ownsFriction);
        }
        if(desc.motion == Dynamic && desc.mass > 0.0f) {
            settings.mOverrideMassProperties =
                JPH::EOverrideMassProperties::CalculateInertia;
            settings.mMassPropertiesOverride.mMass = desc.mass;
        }

        const auto serial = u64{impl->rowBySerial.size()};
        settings.mUserData = serial;

        const auto id = impl->Bodies().CreateAndAddBody(
            settings,
            desc.motion == Static ? JPH::EActivation::DontActivate
                                  : JPH::EActivation::Activate
        );
        if(id.IsInvalid()) {
            LOG_ERROR("the world is full: no room for another body");
            CROWY_ASSERT(false, "the world is full: raise maxBodies");
            return BodyHandle{};
        }

        const auto handle = impl->bodies.Add(BodyRow{
            .id = id,
            .serial = serial,
            .motion = desc.motion,
            .shape = desc.shape,
            .mass = desc.motion == Dynamic ? settings.GetMassProperties().mMass
                                           : 0.0f,
        });
        impl->rowBySerial.push_back(handle);

        return impl->PublicHandle(handle);
    }

    HingeHandle PhysicsWorld::CreateHinge(const HingeDesc& desc) {
        constexpr auto Pi = std::numbers::pi_v<f32>;

        const auto& row = impl->Row(desc.body);
        CROWY_ASSERT(
            row.motion == BodyMotion::Dynamic,
            "a hinge holds a Dynamic body"
        );
        CROWY_ASSERT(
            desc.minAngle >= -Pi && desc.minAngle <= 0.0f,
            "minAngle {} is outside [-pi, 0]",
            desc.minAngle
        );
        CROWY_ASSERT(
            desc.maxAngle >= 0.0f && desc.maxAngle <= Pi,
            "maxAngle {} is outside [0, pi]",
            desc.maxAngle
        );
        CROWY_ASSERT(desc.minAngle < desc.maxAngle, "the limits are empty");
        CROWY_ASSERT(
            toJolt(desc.axis).IsNormalized() &&
                toJolt(desc.normal).IsNormalized() &&
                std::abs(toJolt(desc.axis).Dot(toJolt(desc.normal))) < 1.0e-4f,
            "a hinge needs a unit axis and a unit normal perpendicular to it"
        );

        JPH::HingeConstraintSettings settings;
        settings.SetEmbedded();
        settings.mSpace = JPH::EConstraintSpace::WorldSpace;
        settings.mPoint1 = settings.mPoint2 = toJolt(desc.pivot);
        settings.mHingeAxis1 = settings.mHingeAxis2 = toJolt(desc.axis);
        settings.mNormalAxis1 = settings.mNormalAxis2 = toJolt(desc.normal);
        settings.mLimitsMin = desc.minAngle;
        settings.mLimitsMax = desc.maxAngle;
        settings.mMaxFrictionTorque = desc.maxFrictionTorque;

        JPH::BodyLockWrite lock(
            impl->system.GetBodyLockInterfaceNoLock(),
            row.id
        );
        CROWY_ASSERT(lock.Succeeded());
        // Jolt is built without RTTI, and a hinge's settings make a hinge
        HingeConstraintRef constraint =
            static_cast<JPH::HingeConstraint*>(
                settings.Create(JPH::Body::sFixedToWorld, lock.GetBody())
            );
        lock.ReleaseLock();
        impl->system.AddConstraint(constraint.GetPtr());

        return impl->PublicHandle(impl->hinges.Add(HingeRow{constraint}));
    }

    void PhysicsWorld::AddImpulse(BodyHandle body, Vec3 impulse) {
        const auto& row = impl->Row(body);
        CROWY_ASSERT(
            row.motion == BodyMotion::Dynamic,
            "an impulse moves a Dynamic body"
        );

        impl->Bodies().AddImpulse(row.id, toJolt(impulse));
    }

    void PhysicsWorld::AddImpulseAt(
        BodyHandle body,
        Vec3 impulse,
        Vec3 worldPoint
    ) {
        const auto& row = impl->Row(body);
        CROWY_ASSERT(
            row.motion == BodyMotion::Dynamic,
            "an impulse moves a Dynamic body"
        );

        impl->Bodies().AddImpulse(row.id, toJolt(impulse), toJolt(worldPoint));
    }

    void PhysicsWorld::MoveKinematic(BodyHandle body, const BodyPose& target) {
        auto& row = impl->Row(body);
        CROWY_ASSERT(
            row.motion == BodyMotion::Kinematic,
            "MoveKinematic moves a Kinematic body"
        );
        CROWY_ASSERT(isUnit(target.rotation), "a pose needs a unit rotation");

        row.kinematicTarget = target;
    }

    void PhysicsWorld::ReleaseHinge(HingeHandle hinge) {
        const auto& row = impl->Row(hinge);
        row.constraint->SetEnabled(false);
        impl->Bodies().ActivateBody(row.constraint->GetBody2()->GetID());
    }

    void PhysicsWorld::AddWater(const WaterDesc& water) {
        CROWY_ASSERT(
            water.halfExtent.x > 0.0f && water.halfExtent.y > 0.0f &&
                water.halfExtent.z > 0.0f,
            "water needs a positive half extent"
        );
        CROWY_ASSERT(water.density > 0.0f, "water needs a density");

        impl->waters.push_back(water);
    }

    void PhysicsWorld::Step() {
        if(!std::exchange(impl->broadPhaseOptimized, true))
            impl->system.OptimizeBroadPhase();

        // Jolt measures how much of each body is under the surface; the
        // buoyancy it takes is the water's density over the body's
        const auto gravity = impl->system.GetGravity();
        for(const auto& water: impl->waters) {
            const auto surface =
                water.center + Vec3{0.0f, water.halfExtent.y, 0.0f};
            for(const auto body: Overlapping(water.center, water.halfExtent)) {
                const auto& row = impl->Row(body);
                if(row.motion != BodyMotion::Dynamic)
                    continue;
                // the Body call, unlike the interface's, wakes nothing, so a
                // body at rest in the water can sleep
                JPH::BodyLockWrite lock(
                    impl->system.GetBodyLockInterfaceNoLock(),
                    row.id
                );
                if(!lock.Succeeded() || !lock.GetBody().IsActive())
                    continue;
                auto& jolt = lock.GetBody();
                jolt.ApplyBuoyancyImpulse(
                    toJolt(surface),
                    JPH::Vec3::sAxisY(),
                    water.density * jolt.GetShape()->GetVolume() / row.mass,
                    water.linearDrag,
                    water.angularDrag,
                    JPH::Vec3::sZero(),
                    gravity,
                    PhysicsTickSeconds
                );
            }
        }

        // the velocity MoveKinematic sets persists, so a reached target is
        // asked for again and the velocity drops to zero
        auto& interface = impl->Bodies();
        for(const auto handle: impl->rowBySerial) {
            if(!impl->bodies.IsValid(handle))
                continue;
            const auto& row = impl->bodies.GetRef(handle);
            if(!row.kinematicTarget)
                continue;
            interface.MoveKinematic(
                row.id,
                toJolt(row.kinematicTarget->position),
                toJoltQuat(row.kinematicTarget->rotation),
                PhysicsTickSeconds
            );
        }

        const auto errors = impl->system.Update(
            PhysicsTickSeconds,
            1,
            &impl->runtime.tempAllocator,
            &impl->runtime.jobSystem
        );
        if(errors != JPH::EPhysicsUpdateError::None) {
            LOG_ERROR(
                "contacts were dropped at tick {}: {:#x}",
                impl->tickCount + 1,
                static_cast<u32>(errors)
            );
            CROWY_ASSERT(false, "a physics cache is full");
        }
        ++impl->tickCount;
    }

    bool PhysicsWorld::IsValid(BodyHandle body) const noexcept {
        return impl->RowHandle(body).has_value();
    }

    bool PhysicsWorld::IsValid(HingeHandle hinge) const noexcept {
        return impl->RowHandle(hinge).has_value();
    }

    u64 PhysicsWorld::TickCount() const noexcept {
        return impl->tickCount;
    }

    usize PhysicsWorld::BodyCount() const noexcept {
        return impl->bodies.Count();
    }

    BodyMotion PhysicsWorld::MotionOf(BodyHandle body) const {
        return impl->Row(body).motion;
    }

    BodyShape PhysicsWorld::ShapeOf(BodyHandle body) const {
        return impl->Row(body).shape;
    }

    BodyPose PhysicsWorld::PoseOf(BodyHandle body) const {
        JPH::RVec3 position;
        JPH::Quat rotation;
        impl->Bodies().GetPositionAndRotation(
            impl->Row(body).id,
            position,
            rotation
        );

        return BodyPose{toVec3(position), toVec4(rotation)};
    }

    Vec3 PhysicsWorld::LinearVelocityOf(BodyHandle body) const {
        return toVec3(impl->Bodies().GetLinearVelocity(impl->Row(body).id));
    }

    f32 PhysicsWorld::MassOf(BodyHandle body) const {
        const auto& row = impl->Row(body);
        CROWY_ASSERT(
            row.motion == BodyMotion::Dynamic,
            "only a Dynamic body has a mass"
        );

        return row.mass;
    }

    bool PhysicsWorld::IsAwake(BodyHandle body) const {
        return impl->Bodies().IsActive(impl->Row(body).id);
    }

    f32 PhysicsWorld::HingeAngleOf(HingeHandle hinge) const {
        return impl->Row(hinge).constraint->GetCurrentAngle();
    }

    bool PhysicsWorld::IsHingeHeld(HingeHandle hinge) const {
        return impl->Row(hinge).constraint->GetEnabled();
    }

    BodyHandles PhysicsWorld::Overlapping(Vec3 center, Vec3 halfExtent) const {
        using ShapeHits =
            JPH::AllHitCollisionCollector<JPH::CollideShapeCollector>;

        CROWY_ASSERT(
            halfExtent.x > 0.0f && halfExtent.y > 0.0f && halfExtent.z > 0.0f,
            "a query box needs a positive half extent"
        );

        // radius 0: the default convex radius would round the query's corners
        JPH::BoxShapeSettings box(toJolt(halfExtent), 0.0f);
        box.SetEmbedded();
        const auto shape = box.Create().Get();

        ShapeHits hits;
        impl->system.GetNarrowPhaseQueryNoLock().CollideShape(
            shape,
            JPH::Vec3::sOne(),
            JPH::RMat44::sTranslation(toJolt(center)),
            JPH::CollideShapeSettings{},
            JPH::RVec3::sZero(),
            hits,
            JPH::SpecifiedBroadPhaseLayerFilter(MovingBroadPhase),
            JPH::SpecifiedObjectLayerFilter(MovingLayer)
        );

        // hits arrive in an order the broad phase decides
        std::vector<u64> serials;
        serials.reserve(hits.mHits.size());
        for(const auto& hit: hits.mHits)
            serials.push_back(impl->Bodies().GetUserData(hit.mBodyID2));
        std::ranges::sort(serials);
        const auto [first, last] = std::ranges::unique(serials);
        serials.erase(first, last);

        BodyHandles found;
        found.reserve(serials.size());
        for(const auto serial: serials) {
            const auto handle = impl->rowBySerial[serial];
            if(impl->bodies.IsValid(handle))
                found.push_back(impl->PublicHandle(handle));
        }

        return found;
    }

    u64 PhysicsWorld::StateHash() const {
        std::array<JPH::uint8, sizeof(u64)> tick{};
        for(usize i = 0; i < tick.size(); ++i)
            tick[i] = static_cast<JPH::uint8>(impl->tickCount >> (8 * i));

        JPH::StateRecorderImpl recorder;
        impl->system.SaveState(recorder);
        const auto state = recorder.GetData();

        return JPH::HashBytes(
            state.data(),
            static_cast<JPH::uint>(state.size()),
            JPH::HashBytes(tick.data(), static_cast<JPH::uint>(tick.size()))
        );
    }
}
