#pragma once

#include <array>
#include <filesystem>
#include <vector>

#include "LinearAlgebra.hpp"
#include "MeshData.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    // glTF 2.0 metallic-roughness; an empty map path leaves the constant factor
    // alone
    struct Material {
        Vec3 albedo{1.0f, 1.0f, 1.0f};
        f32 metallic = 0.0f;
        f32 roughness = 0.5f;
        Vec3 emissive{};

        std::filesystem::path albedoMap;
    };

    struct SceneObject {
        MeshData mesh;
        Mat4 transform = unitMat();
        Material material;
    };

    // spans center +/- halfExtent.x * tangent +/- halfExtent.y * bitangent
    struct AreaLight {
        Vec3 center{};
        Vec3 tangent{1.0f, 0.0f, 0.0f};
        Vec3 bitangent{0.0f, 0.0f, 1.0f};
        Vec2 halfExtent{1.0f, 1.0f};
        Vec3 normal{0.0f, -1.0f, 0.0f};
        Vec3 radiance{1.0f, 1.0f, 1.0f};
    };

    struct SceneData {
        std::vector<SceneObject> objects;
        std::vector<AreaLight> lights;
        std::filesystem::path skybox;
    };

    // +X, -X, +Y, -Y, +Z, -Z
    std::array<std::filesystem::path, 6> SkyboxFacePaths(
        const std::filesystem::path& dir
    );
}
