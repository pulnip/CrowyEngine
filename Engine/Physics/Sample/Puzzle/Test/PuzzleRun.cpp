#include "PuzzleRun.hpp"

#include <algorithm>
#include <variant>

namespace
{
    using namespace Crowy;

    AABB3D unite(const AABB3D& a, const AABB3D& b) {
        const auto aLow = a.center - a.halfScale;
        const auto aHigh = a.center + a.halfScale;
        const auto bLow = b.center - b.halfScale;
        const auto bHigh = b.center + b.halfScale;
        const auto low = Vec3{
            std::min(aLow.x, bLow.x),
            std::min(aLow.y, bLow.y),
            std::min(aLow.z, bLow.z),
        };
        const auto high = Vec3{
            std::max(aHigh.x, bHigh.x),
            std::max(aHigh.y, bHigh.y),
            std::max(aHigh.z, bHigh.z),
        };

        return AABB3D{
            .center = (low + high) * 0.5f,
            .halfScale = (high - low) * 0.5f,
        };
    }

    AABB3D turnedBox(Vec3 halfExtent, const Mat4& placement) {
        return transformAABB3D(
            placement,
            AABB3D{.center = zeros(), .halfScale = halfExtent}
        );
    }
}

namespace Crowy
{
    void runTo(PuzzleSession& session, u64 tick) {
        while(session.GetStatus().tick < tick)
            session.Tick();
    }

    TickHashes hashesToHorizon(PuzzleSession& session) {
        TickHashes hashes{session.GetStatus().hash};
        while(session.GetStatus().tick < PuzzleHorizon) {
            session.Tick();
            hashes.push_back(session.GetStatus().hash);
        }

        return hashes;
    }

    AABB3D boundsOf(const BodyShape& shape, const BodyPose& pose) {
        if(const auto* sphere = std::get_if<SphereShape>(&shape)) {
            return AABB3D{
                .center = pose.position,
                .halfScale = ones() * sphere->radius,
            };
        }

        const auto placement = modelMat(pose.position, pose.rotation, ones());
        if(const auto* box = std::get_if<BoxShape>(&shape))
            return turnedBox(box->halfExtent, placement);

        const auto& parts = std::get<CompoundShape>(shape).parts;
        auto bounds = AABB3D{.center = pose.position};
        for(usize i = 0; i < parts.size(); ++i) {
            const auto& part = parts[i];
            const auto partBounds = turnedBox(
                part.halfExtent,
                placement *
                    modelMat(part.pose.position, part.pose.rotation, ones())
            );
            bounds = i == 0 ? partBounds : unite(bounds, partBounds);
        }

        return bounds;
    }

    AABB3D worldBoundsOf(const PhysicsWorld& world, BodyHandle body) {
        return boundsOf(world.ShapeOf(body), world.PoseOf(body));
    }
}
