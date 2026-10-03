#include "RenderScene.hpp"

#include "RHIDevice.hpp"

namespace Crowy
{
    void RenderScene::RetireTextures(RHIDevice& device) {
        while(!textures.IsEmpty()) {
            const auto handle = textures.HandleAt(textures.Count() - 1);
            device.Retire(std::move(textures.GetRef(handle).texture));
            textures.Remove(handle);
        }
    }
}
