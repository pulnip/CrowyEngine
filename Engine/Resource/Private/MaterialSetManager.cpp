#include "LoadContext.hpp"
#include "MaterialSetManager.hpp"
#include "RHIDevice.hpp"
#include "RHITexture.hpp"
#include "TextureImporter.hpp"

namespace Crowy
{
    MaterialSetManager* MaterialSetManager::instance = nullptr;

    MaterialSet instantiate(const MaterialSetRequest& request, LoadContext& ctx){
        TextureMap textureMap;

        for(const auto& [slot, matDesc]: request.data){
            for(const auto& [usage, texDesc]: matDesc.textures){
                auto textureData = importTexture(texDesc.uri);
                if(!textureData.has_value())
                    continue;

                auto texture = ctx.device->createTexture(
                    RHITextureCreateDesc{
                        .width = textureData->getWidth(),
                        .height = textureData->getHeight(),
                        .depth = 1,
                        .mipLevels = 1,
                        .arraySize = 1,
                        .format = RHITextureFormat::RGBA8_UNORM,
                        .usage = RHITextureUsageFlags::TEX_ShaderResource,
                        .initialState = RHIResourceState::ShaderResource,
                        .clearColor = {},
                        .clearDepthStencil = {},
                        .initialData = textureData->pixels.data()
                    }
                );

                textureMap.emplace(slot, std::move(texture));
            }
        }

        return MaterialSet{
            .materials = std::move(textureMap)
        };
    }
}