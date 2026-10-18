#include "RenderScene.hpp"

#include <utility>

#include "Assert.hpp"
#include "RHIDevice.hpp"

namespace Crowy
{
    MeshResource meshResourceOf(
        MeshAssetHandle asset,
        MaterialHandles materials
    ) {
        CROWY_ASSERT(
            materials.size() == asset->slots.size(),
            "a mesh asset with {} slots given {} materials",
            asset->slots.size(),
            materials.size()
        );

        MeshResource mesh{
            .materials = std::move(materials),
            .localBounds = asset->bounds
        };
        for(u32 slot = 0; slot < asset->slots.size(); ++slot) {
            mesh.subMeshes.push_back(
                SubMesh{
                    .geometry = asset->slots[slot].geometry,
                    .materialSlot = slot
                }
            );
        }
        mesh.source = std::move(asset);

        return mesh;
    }

    void RenderScene::RetireTextures(RHIDevice& device) {
        while(!textures.IsEmpty()) {
            const auto handle = textures.HandleAt(textures.Count() - 1);
            device.Retire(std::move(textures.GetRef(handle).texture));
            textures.Remove(handle);
        }
    }
}
