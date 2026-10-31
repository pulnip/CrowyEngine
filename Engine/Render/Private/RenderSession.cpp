#include "RenderSession.hpp"

#include <utility>

namespace Crowy
{
    RenderSession::RenderSession(
        RHIDevice& device,
        GeometryPool& pool,
        const AssetRoots& roots
    )
        : meshes(
              [&pool, roots](const AssetRef& ref) {
                  return loadMeshAsset(pool, roots, ref);
              },
              [&pool](MeshAsset asset) {
                  releaseMeshAsset(pool, std::move(asset));
              }
          ),
          textures(
              [&device, roots](const AssetRef& ref) {
                  return loadTextureAsset(device, roots, ref);
              },
              [&device](TextureAsset asset) {
                  releaseTextureAsset(device, std::move(asset));
              }
          ),
          materials(
              [this, roots](const AssetRef& ref) {
                  return loadMaterialAsset(scene, textures, roots, ref);
              },
              [this](MaterialAsset asset) {
                  releaseMaterialAsset(scene, std::move(asset));
              }
          ) {}

    void RenderSession::Collect() {
        meshes.Collect();
        materials.Collect();
        textures.Collect();
    }
}
