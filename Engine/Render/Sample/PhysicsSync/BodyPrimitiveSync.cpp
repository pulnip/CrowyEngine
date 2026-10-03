#include "BodyPrimitiveSync.hpp"

#include <algorithm>

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
            primitive.localToWorld = offset *
                modelMat(pose.position, pose.rotation, binding.meshScale);
            primitive.worldBounds =
                transformAABB3D(primitive.localToWorld, binding.localBounds);
        }
    }
}
