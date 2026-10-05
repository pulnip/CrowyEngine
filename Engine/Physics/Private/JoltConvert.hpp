#pragma once

// Jolt.h comes before every other Jolt header, in every file
#include <Jolt/Jolt.h>

#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Vec3.h>

#include "Primitives.hpp"

// component by component: Jolt's vectors are 16-byte SIMD values
namespace Crowy
{
    inline JPH::Vec3 toJolt(Vec3 v) noexcept {
        return JPH::Vec3(v.x, v.y, v.z);
    }

    inline JPH::Quat toJoltQuat(Vec4 q) noexcept {
        return JPH::Quat(q.x, q.y, q.z, q.w);
    }

    inline Vec3 toVec3(JPH::Vec3Arg v) noexcept {
        return Vec3{v.GetX(), v.GetY(), v.GetZ()};
    }

    inline Vec4 toVec4(JPH::QuatArg q) noexcept {
        return Vec4{q.GetX(), q.GetY(), q.GetZ(), q.GetW()};
    }
}
