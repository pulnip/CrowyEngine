#pragma once

#include <optional>
#include <variant>
#include <vector>

#include "Geometry/Overlap3D.hpp"
#include "PhysicsTypes.hpp"
#include "PhysicsWorld.hpp"
#include "Primitives.hpp"
#include "RenderScene.hpp"

namespace Crowy
{
    struct BodyBinding;

    using BodyBindings = std::vector<BodyBinding>;

    // MakeBox(0.5f) and MakeSphere(0.5f): a shape's scale stretches them
    inline constexpr AABB3D UnitMeshBounds{.halfScale = {0.5f, 0.5f, 0.5f}};

    // a compound's parts carry their own scale
    inline constexpr Vec3 unitMeshScaleOf(const BodyShape& shape) noexcept {
        if(const auto* box = std::get_if<BoxShape>(&shape))
            return 2.0f * box->halfExtent;
        if(const auto* sphere = std::get_if<SphereShape>(&shape))
            return 2.0f * sphere->radius * ones();

        return ones();
    }

    struct BodyBinding {
        BodyHandle body;
        PrimitiveHandle primitive;
        // where a compound's part sits in its body; empty for a plain body
        std::optional<BodyPose> part;
        // the mesh's bounds before meshScale
        AABB3D localBounds = UnitMeshBounds;
        Vec3 meshScale = ones();
    };

    // One world's bodies written into primitives, moved by where the world
    // stands in the scene; the only writer of a bound primitive.
    class BodyPrimitiveSync {
    private:
        Vec3 placement = zeros();
        BodyBindings bindings;

    public:
        explicit BodyPrimitiveSync(Vec3 placement)
            : placement(placement) {}

        // a primitive is bound once: one writer
        void Bind(const BodyBinding& binding);
        // after the frame's ticks; localToWorld and worldBounds together
        void Sync(const PhysicsWorld& world, RenderScene& scene) const;
    };
}
