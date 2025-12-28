#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <vector>
#include "ComponentDefinitions.hpp"

namespace Crowy
{
    struct MaterialSpec{
        std::string baseColor;
        std::string targetSlot;
    };
    struct RenderObjectSpec{
        std::string uri;
        std::vector<MaterialSpec> material_override;
        std::string renderType;
    };

    using TransformComponents = std::vector<TransformComponent>;
    using MaterialSpecs = std::vector<MaterialSpec>;
    using RenderObjectSpecs = std::vector<RenderObjectSpec>;
    using RigidbodyComponents = std::vector<RigidbodyComponent>;
    using BoxColliderComponents = std::vector<BoxColliderComponent>;
    using SphereColliderComponents = std::vector<SphereColliderComponent>;
    using CameraComponents = std::vector<CameraComponent>;
    using PlayerComponents = std::vector<PlayerComponent>;
    using EditorComponents = std::vector<EditorComponent>;

    constexpr auto INVALID_INDEX = std::numeric_limits<uint32_t>::max();

    struct EntitySpec{
        std::string name;
        uint32_t transformIndex      = INVALID_INDEX;
        uint32_t renderObjectIndex   = INVALID_INDEX;
        uint32_t rigidbodyIndex      = INVALID_INDEX;
        uint32_t boxColliderIndex    = INVALID_INDEX;
        uint32_t sphereColliderIndex = INVALID_INDEX;
        uint32_t cameraIndex         = INVALID_INDEX;
        uint32_t playerIndex         = INVALID_INDEX;
        uint32_t editorIndex         = INVALID_INDEX;
    };

    using EntitySpecs = std::vector<EntitySpec>;

    struct SceneSpec{
        // SoA
        TransformComponents      transformSpecs;
        RenderObjectSpecs        renderObjectSpecs;
        RigidbodyComponents      rigidbodySpecs;
        BoxColliderComponents    boxColliderSpecs;
        SphereColliderComponents sphereColliderSpecs;
        CameraComponents         cameraSpecs;
        PlayerComponents         playerSpecs;
        EditorComponents         editorSpecs;

        EntitySpecs entities;
    };
}