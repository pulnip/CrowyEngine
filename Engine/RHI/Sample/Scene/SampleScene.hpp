#pragma once

#include <array>
#include <filesystem>
#include <vector>
#include "LinearAlgebra.hpp"
#include "MeshData.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    // Ready-made scenes for the samples to render, so that a sample testing a
    // shading technique does not have to build geometry of its own.
    //
    // These are fixtures, not an engine feature: nothing here is loaded from
    // disk or serialized, and the types below are the smallest vocabulary the
    // samples need rather than a scene format. When a real scene module lands,
    // the types graduate there and the two builders stay behind.

    // Metallic-roughness PBR, following the glTF 2.0 parameterization.
    // An empty map path means the constant factor applies on its own, which is
    // how a sample that ignores textures entirely still gets sensible values.
    struct Material{
        Vec3 albedo{1.0f, 1.0f, 1.0f};
        f32 metallic = 0.0f;
        f32 roughness = 0.5f;
        Vec3 emissive{};

        std::filesystem::path albedoMap;
        std::filesystem::path normalMap;
        // metallic in B and roughness in G, per the glTF convention
        std::filesystem::path metallicRoughnessMap;
    };

    struct SceneObject{
        MeshData mesh;
        Mat4 transform = unitMat();
        Material material;
    };

    // A rectangular light lying in a plane, emitting from the face it faces.
    // The rectangle spans center +/- halfExtent.x*tangent +/- halfExtent.y*bitangent.
    struct AreaLight{
        Vec3 center{};
        Vec3 tangent{1.0f, 0.0f, 0.0f};
        Vec3 bitangent{0.0f, 0.0f, 1.0f};
        Vec2 halfExtent{1.0f, 1.0f};
        Vec3 normal{0.0f, -1.0f, 0.0f};
        // radiance in arbitrary units; samples scale it to taste
        Vec3 radiance{1.0f, 1.0f, 1.0f};
    };

    struct SceneData{
        std::vector<SceneObject> objects;
        std::vector<AreaLight> lights;
        // directory holding the 6 cubemap faces, empty when the scene has none
        std::filesystem::path skybox;
    };

    // The 6 face files of a skybox directory, in the order a cubemap array
    // expects them: +X, -X, +Y, -Y, +Z, -Z.
    std::array<std::filesystem::path, 6> SkyboxFacePaths(const std::filesystem::path& dir);
}
