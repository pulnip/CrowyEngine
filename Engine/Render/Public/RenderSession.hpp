#pragma once

#include "AssetRoots.hpp"
#include "GeometryPool.hpp"
#include "MaterialAsset.hpp"
#include "MeshAsset.hpp"
#include "RHIFWD.hpp"
#include "RenderScene.hpp"
#include "Semantics.hpp"
#include "TextureAsset.hpp"

namespace Crowy
{
    // The scene and the stores that fill it, destroyed together. It must die
    // before the device it retires into and the pool its meshes free into.
    class RenderSession {
    private:
        // after the pool it frees into, before the scene whose rows hold it
        MeshStore meshes;
        // after the device it retires into, before the materials that hold it
        TextureStore textures;
        RenderScene scene;
        // after the scene whose material rows it removes
        MaterialStore materials;

    public:
        ~RenderSession() = default;
        CROWY_DECLARE_PINNED(RenderSession)

        RenderSession(
            RHIDevice& device,
            GeometryPool& pool,
            const AssetRoots& roots
        );

        // what the scene's own Sync let go leaves with this call
        void Collect();

        auto& Meshes() noexcept { return meshes; }
        auto& Textures() noexcept { return textures; }
        auto& Materials() noexcept { return materials; }
        auto& Scene() noexcept { return scene; }
        const auto& Scene() const noexcept { return scene; }
    };
}
