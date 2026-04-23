#include <utility>
#include "LoadContext.hpp"
#include "MaterialSetManager.hpp"
#include "RHIDevice.hpp"
#include "RHITexture.hpp"
#include "TextureImporter.hpp"

namespace Crowy
{
    MaterialSetManager* MaterialSetManager::instance = nullptr;

    static RHITexturePtr instantiate(const TextureRef& ref, LoadContext& ctx){
        auto textureData = importTexture(ref.uri);
        if(!textureData.has_value())
            return nullptr;

        // TODO: Implement mipmap generation
        // For now, use only 1 mip level to avoid sampling uninitialized mipmap data
        uint32_t mipLevels = 1;
        // if(has_flag(ref.flags, TextureFlags::GenerateMips)){
        //     mipLevels = static_cast<uint32_t>(
        //         std::floor(std::log2(std::max(
        //             textureData->width, textureData->height
        //         )))
        //     ) + 1;
        // }

        RHIPixelFormat format = has_flag(ref.flags, TextureFlags::SRGB) 
            ? RHIPixelFormat::RGBA8_UNORM_SRGB 
            : RHIPixelFormat::RGBA8_UNORM;

        RHITextureCreateDesc texDesc{
            .width = textureData->getWidth(),
            .height = textureData->getHeight(),
            .depth = 1,
            .mipLevels = mipLevels,
            .arraySize = 1,
            .format = format,
            .usage = RHITextureUsage::AllowShaderRead,
            .initialState = RHIResourceState::AllShaderResource,
            .clearColor = {},
            .clearDepthStencil = {},
            .initialData = textureData->pixels.data()
        };

    #if defined(_DEBUG) || !defined(NDEBUG)
        return ctx.device->createTexture(texDesc, ref.uri);
    #else
        return ctx.device->createTexture(texDesc);
    #endif
    }

    static Material instantiate(const MaterialRef& ref, LoadContext& ctx){
        Material material{
            .baseColor = ref.baseColor,
            .metallic  = ref.metallic,
            .roughness = ref.roughness,
            .emissive  = ref.emissive,
            .baseColorMap         = nullptr,
            .normalMap            = nullptr,
            .metallicRoughnessMap = nullptr,
            .emissiveMap          = nullptr,
            .occlusionMap         = nullptr
        };

        for(const auto& [semantic, texRef]: ref.textures){
            auto texture = instantiate(texRef, ctx);

            if(!texture)
                continue;

            switch(semantic){
            case TextureSemantic::BaseColor:
                material.baseColorMap = std::move(texture);
                break;
            case TextureSemantic::Normal:
                material.normalMap = std::move(texture);
                break;
            case TextureSemantic::MetallicRoughness:
                material.metallicRoughnessMap = std::move(texture);
                break;
            case TextureSemantic::Emissive:
                material.emissiveMap = std::move(texture);
                break;
            case TextureSemantic::Occlusion:
                material.occlusionMap = std::move(texture);
                break;
            default:
                std::unreachable();
            }
        }

        return material;
    }

    MaterialSet instantiate(const MaterialSetRequest& request, LoadContext& ctx){
        MaterialMap materialMap;

        for(const auto& [slot, matRef]: request.data){
            materialMap.emplace(slot, instantiate(matRef, ctx));
        }

        return MaterialSet{
            .materials = std::move(materialMap)
        };
    }
}