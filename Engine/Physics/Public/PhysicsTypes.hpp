#pragma once

#include <numbers>
#include <variant>
#include <vector>

#include "GenericHandle.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    struct BodyTag;
    struct HingeTag;
    struct BoxShape;
    struct SphereShape;

    // valid only in the world that made it
    using BodyHandle = GenericHandle<BodyTag>;
    using HingeHandle = GenericHandle<HingeTag>;
    using BodyHandles = std::vector<BodyHandle>;
    using BodyShape = std::variant<BoxShape, SphereShape>;

    // one Step; nothing in physics takes a frame's time
    inline constexpr f32 PhysicsTickSeconds = 1.0f / 60.0f;

    enum class BodyMotion : u8 {
        Static,
        Kinematic,
        Dynamic,
    };

    struct BoxShape {
        Vec3 halfExtent{0.5f, 0.5f, 0.5f};
    };

    struct SphereShape {
        f32 radius = 0.5f;
    };

    // the body origin, never the center of mass; rotation is a unit xyzw
    struct BodyPose {
        Vec3 position = zeros();
        Vec4 rotation = unitQuat();
    };

    struct BodyDesc {
        BodyShape shape = BoxShape{};
        BodyPose pose;
        BodyMotion motion = BodyMotion::Dynamic;
        // 0 takes the shape's mass at 1000 kg/m^3
        f32 mass = 0.0f;
        f32 friction = 0.2f;
        f32 restitution = 0.0f;
        f32 linearDamping = 0.05f;
        f32 angularDamping = 0.05f;
    };

    // a dynamic body hinged to the world, in world space at creation, where
    // the angle reads 0; minAngle in [-pi, 0], maxAngle in [0, pi]
    struct HingeDesc {
        BodyHandle body;
        Vec3 pivot = zeros();
        Vec3 axis = unitY();
        // perpendicular to axis
        Vec3 normal = unitX();
        f32 minAngle = -std::numbers::pi_v<f32>;
        f32 maxAngle = std::numbers::pi_v<f32>;
        f32 maxFrictionTorque = 0.0f;
    };
}
