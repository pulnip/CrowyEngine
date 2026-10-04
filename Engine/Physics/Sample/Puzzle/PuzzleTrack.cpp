#include "Puzzle.hpp"

#include <cmath>

#include "Assert.hpp"

// Explicit arithmetic in a contraction-off TU: an inline Core helper would
// take the flags of whichever file the linker kept it from.
namespace
{
    using namespace Crowy;

    f32 eased(Easing easing, f32 u) {
        using enum Easing;

        switch(easing) {
        case Linear:
            return u;
        case Smooth:
            return u * u * (3.0f - 2.0f * u);
        case EaseIn:
            return u * u;
        case EaseOut:
            return u * (2.0f - u);
        }
        return u;
    }

    Vec3 lerp(Vec3 a, Vec3 b, f32 s) {
        return Vec3{
            a.x + (b.x - a.x) * s,
            a.y + (b.y - a.y) * s,
            a.z + (b.z - a.z) * s,
        };
    }

    // the shorter way round, renormalized with one correctly rounded sqrt
    Vec4 nlerp(Vec4 a, Vec4 b, f32 s) {
        const auto cosine = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
        const auto sign = cosine < 0.0f ? -1.0f : 1.0f;
        const auto keep = 1.0f - s;
        const auto q = Vec4{
            a.x * keep + sign * b.x * s,
            a.y * keep + sign * b.y * s,
            a.z * keep + sign * b.z * s,
            a.w * keep + sign * b.w * s,
        };
        const auto length =
            std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);

        return Vec4{q.x / length, q.y / length, q.z / length, q.w / length};
    }
}

namespace Crowy
{
    BodyPose poseAt(const PoseTrack& track, const BodyPose& start, u64 tick) {
        auto from = PoseKey{.tick = 0, .pose = start};
        for(const auto& key: track.keys) {
            CROWY_ASSERT(key.tick > from.tick, "a track's keys rise");
            if(tick >= key.tick) {
                from = key;
                continue;
            }
            if(tick == from.tick)
                return from.pose;

            const auto u = static_cast<f32>(tick - from.tick) /
                static_cast<f32>(key.tick - from.tick);
            const auto s = eased(key.easing, u);

            // a hold keeps its rotation exactly: nlerp would round it anew
            // each tick
            const auto& rotation = from.pose.rotation;
            return BodyPose{
                .position = lerp(from.pose.position, key.pose.position, s),
                .rotation = rotation == key.pose.rotation
                    ? rotation
                    : nlerp(rotation, key.pose.rotation, s),
            };
        }

        return from.pose;
    }
}
