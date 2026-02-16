#pragma once

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
    };

    struct CameraComponent{
        float fov;
        float nearPlane;
        float farPlane;
        Projection proj;
        RHIViewport viewport;
        // TODO. multi window support
        // add RenderTarget(RHITexture) info later.
    };

    struct ColorComponent{
        Vec4 color;
    };

    struct RenderObjectComponent{
        MeshHandle mesh;
        MaterialSetHandle materialSet;
        RenderTypeHash renderType;
    };

    struct RigidbodyComponent{
        Vec3 velocity;
        bool useGravity;
        float mass;
    };
    struct SphereColliderComponent{
        Vec3 position;
        float radius;

        // physical material
        float bounciness;
        float friction;
    };
    struct BoxColliderComponent{
        Vec3 position = zeros();
        Vec4 rotation = unitQuat();
        Vec3 scale = ones();

        // physical material
        float bounciness;
        float friction;
    };

    // Entity-to-Entity Event Tags
    struct ImpulseComponent{
        Vec3 force;
        float dt;
    };

    struct ScriptComponent{
        ScriptHandle handle;
    };

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
        X(        PlayerComponent) \
        X(        EditorComponent) \
        X(    AttachableComponent)
}