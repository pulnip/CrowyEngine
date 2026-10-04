#include "BodyPrimitiveSync.hpp"

#include <algorithm>
#include <variant>

#include "Assert.hpp"
#include "LinearAlgebra.hpp"

namespace Crowy
{
    void BodyPrimitiveSync::Bind(const BodyBinding& binding) {
        CROWY_ASSERT(!isBound(binding.primitive), "a primitive has one writer");

        bindings.push_back(binding);
    }

    void BodyPrimitiveSync::Bind(const TetherBinding& tether) {
        CROWY_ASSERT(!isBound(tether.primitive), "a primitive has one writer");

        tethers.push_back(tether);
    }

    void BodyPrimitiveSync::Sync(
        const PhysicsWorld& world,
        RenderScene& scene
    ) const {
        const auto offset = translateMat(placement);
        for(const auto& binding: bindings) {
            const auto pose = world.PoseOf(binding.body);
            auto& primitive = scene.Primitives().GetRef(binding.primitive);
            primitive.localToWorld = binding.part
                ? offset * modelMat(pose.position, pose.rotation, ones()) *
                    modelMat(
                        binding.part->position,
                        binding.part->rotation,
                        binding.meshScale
                    )
                : offset *
                    modelMat(pose.position, pose.rotation, binding.meshScale);
            // a sphere's bounds do not turn with it, so a rolling ball
            // never reaches below the floor and moves the shadow fit
            const auto shape = world.ShapeOf(binding.body);
            if(std::holds_alternative<SphereShape>(shape)) {
                const auto center = primitive.localToWorld *
                    toVec4(binding.localBounds.center, 1.0f);
                primitive.worldBounds = AABB3D{
                    .center = static_cast<Vec3>(center),
                    .halfScale =
                        binding.localBounds.halfScale * binding.meshScale,
                };
                continue;
            }
            primitive.worldBounds =
                transformAABB3D(primitive.localToWorld, binding.localBounds);
        }
        for(const auto& tether: tethers)
            syncTether(tether, world, scene);
    }

    bool BodyPrimitiveSync::isBound(PrimitiveHandle primitive) const {
        const auto holds = [&](const auto& list, auto member) {
            return std::ranges::find(list, primitive, member) != list.end();
        };

        return holds(bindings, &BodyBinding::primitive) ||
            holds(tethers, &TetherBinding::primitive);
    }

    // released, it hides where it last stood
    void BodyPrimitiveSync::syncTether(
        const TetherBinding& tether,
        const PhysicsWorld& world,
        RenderScene& scene
    ) const {
        auto& primitive = scene.Primitives().GetRef(tether.primitive);
        if(!world.IsHingeHeld(tether.hinge)) {
            primitive.flags = PrimitiveFlags::None;
            return;
        }

        const auto end = world.PoseOf(tether.body).position;
        const auto span = end - tether.anchor;
        const auto across =
            normalize(cross(span, tether.axis)) * tether.thickness;
        primitive.localToWorld = Mat4{
            toVec4(across, 0.0f),
            toVec4(span, 0.0f),
            toVec4(tether.axis * tether.thickness, 0.0f),
            toVec4(placement + (tether.anchor + end) * 0.5f, 1.0f),
        };
        primitive.worldBounds =
            transformAABB3D(primitive.localToWorld, UnitMeshBounds);
        primitive.flags =
            combine(PrimitiveFlags::Visible, PrimitiveFlags::CastShadow);
    }
}
