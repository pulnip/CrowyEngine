#pragma once

#include <type_traits>
#include "math.hpp"
#include "ECSDefinitions.hpp"
#include "RenderDefinitions.hpp"
#include "ResourceHandle.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    struct TransformComponent{
        Vec3 position = zeros();
        Vec4 rotation = unitQuat();
        Vec3 scale = ones();

        Vec3 right(){ return Crowy::right(rotation); }
        Vec3 up(){ return Crowy::up(rotation); }
        Vec3 forward(){ return Crowy::forward(rotation); }
    };
    static_assert(std::is_trivially_copyable_v<TransformComponent>);

    struct CameraComponent{
        float fov;
        float nearPlane;
        float farPlane;
        Projection proj;
        RHIViewport viewport;
        // TODO. multi window support
        // add RenderTarget(RHITexture) info later.
    };
    static_assert(std::is_trivially_copyable_v<CameraComponent>);

    struct ColorComponent{
        Vec4 color;
    };
    static_assert(std::is_trivially_copyable_v<ColorComponent>);

    struct RenderObjectComponent{
        MeshHandle mesh;
        MaterialSetHandle materialSet;
        RenderTypeHash renderType;
    };
    static_assert(std::is_trivially_copyable_v<RenderObjectComponent>);

    struct RigidbodyComponent{
        Vec3 velocity;
        bool useGravity;
        float mass;
    };
    static_assert(std::is_trivially_copyable_v<RigidbodyComponent>);

    struct SphereColliderComponent{
        Vec3 position;
        float radius;

        // physical material
        float bounciness;
        float friction;
    };
    static_assert(std::is_trivially_copyable_v<SphereColliderComponent>);

    struct BoxColliderComponent{
        Vec3 position = zeros();
        Vec4 rotation = unitQuat();
        Vec3 scale = ones();

        // physical material
        float bounciness;
        float friction;
    };
    static_assert(std::is_trivially_copyable_v<BoxColliderComponent>);

    // Entity-to-Entity Event Tags
    struct ImpulseComponent{
        Vec3 force;
        float dt;
    };
    static_assert(std::is_trivially_copyable_v<ImpulseComponent>);

    struct ScriptComponent{
        ScriptHandle handle;
    };
    static_assert(std::is_trivially_copyable_v<ScriptComponent>);

    struct CharacterController{
        Vec3 pendingDelta = zeros();

        float slopeLimit = toRadian(45.0);
        float stepOffset = 0.3f;
        float skinWidth = 0.08f;

        void move(Vec3 delta) noexcept{
            pendingDelta += delta;
        }
    };
    static_assert(std::is_trivially_copyable_v<CharacterController>);

    // Entity Type? Property? Tags (kept in Long-term)
    struct PlayerComponent{};
    struct EditorComponent{};
    struct AttachableComponent{};

    #define ARCHETYPES \
        X(     TransformComponent) \
        X(        CameraComponent) \
        X(         ColorComponent) \
        X(  RenderObjectComponent) \
        X(     RigidbodyComponent) \
        X(SphereColliderComponent) \
        X(   BoxColliderComponent) \
        X(       ImpulseComponent) \
        X(        ScriptComponent) \
        X(    CharacterController) \
        X(        PlayerComponent) \
        X(        EditorComponent) \
        X(    AttachableComponent)
}