#include "BodyPrimitiveSync.hpp"

#include <algorithm>
#include <variant>

#include "Assert.hpp"
#include "LinearAlgebra.hpp"

namespace Crowy
{
    void BodyPrimitiveSync::Bind(const BodyBinding& binding) {
        CROWY_ASSERT(
            std::ranges::none_of(
                bindings,
                [&](const BodyBinding& bound) {
                    return bound.primitive == binding.primitive;
                }
            ),
            "a primitive has one writer"
        );

        bindings.push_back(binding);
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
    }
}
