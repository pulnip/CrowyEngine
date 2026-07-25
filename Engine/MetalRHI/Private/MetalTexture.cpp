#include <utility>
#include <Metal/MTLDevice.hpp>
#include "Assert.hpp"
#include "EnumUtil.hpp"
#include "MetalTexture.hpp"
#include "MetalUtil.hpp"
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    namespace{
        auto convert(RHITextureUsage usage){
            using enum RHITextureUsage;
            MTL::TextureUsage mtlUsage = 0;

            if(hasFlag(usage, ShaderRead))
                mtlUsage |= MTL::TextureUsageShaderRead;
            if(hasFlag(usage, RenderTarget))
                mtlUsage |= MTL::TextureUsageRenderTarget;
            if(hasFlag(usage, DepthStencil))
                mtlUsage |= MTL::TextureUsageRenderTarget;
            if(hasFlag(usage, ShaderWrite))
                mtlUsage |= MTL::TextureUsageShaderWrite;

            return mtlUsage;
        }

        auto convert(u32 depth, u32 arraySize){
            CROWY_ASSERT(1 <= depth && depth <= 3,
                "Invalid Texture Depth: {}",
                depth
            );
            CROWY_ASSERT(0 < arraySize,
                "ArraySize should be positive"
            );

            switch(depth){
            case 1:
                return arraySize > 1 ?
                    MTL::TextureType1D :
                    MTL::TextureType1DArray;
            case 2:
                return arraySize > 1 ?
                    MTL::TextureType2D :
                    MTL::TextureType2DArray;
            case 3:
                CROWY_ASSERT(arraySize == 1);
                return MTL::TextureType3D;
            default:
                std::unreachable();
            }
        }
    }

    MetalTexture::MetalTexture(
        MTL::Device& device,
        const RHITextureCreateDesc& desc,
        StrView name
    )
        : RHITexture(
            desc.format,
            RHIBarrierSync::None,
            RHIBarrierAccess::NoAccess,
            RHIBarrierLayout::Undefined,
            desc.mipLevels,
            desc.arraySize
        )
    {
        auto texDesc = MTL::TextureDescriptor::alloc()->init();
        texDesc->setWidth(desc.width);
        texDesc->setHeight(desc.height);
        texDesc->setDepth(desc.depth);
        texDesc->setMipmapLevelCount(desc.mipLevels);
        if(desc.isCubeMap){
            CROWY_ASSERT(desc.depth == 2 && desc.arraySize % 6 == 0);
            texDesc->setArrayLength(desc.arraySize / 6);
            texDesc->setTextureType(desc.arraySize == 6 ?
                MTL::TextureTypeCube :
                MTL::TextureTypeCubeArray
            );
        }
        else{
            texDesc->setArrayLength(desc.arraySize);
            texDesc->setTextureType(convert(desc.depth, desc.arraySize));
        }
        texDesc->setPixelFormat(convert(desc.format));
        texDesc->setUsage(convert(desc.usage));
        texDesc->setStorageMode(MTL::StorageModePrivate);

        texture = device.newTexture(texDesc);
        texDesc->release();

    #if defined(_DEBUG) || !defined(NDEBUG)
        if(!name.empty()){
            texture->setLabel(toNSString(name));
        }
    #endif
    }

    MetalTexture::MetalTexture(
        CA::MetalDrawable* drawable
    )
        : RHITexture(
            convert(
                (texture = drawable->texture())->pixelFormat()
            ),
            RHIBarrierSync::None,
            RHIBarrierAccess::NoAccess,
            RHIBarrierLayout::Present,
            1,
            1
        )
    {
        texture->retain();
    }

    MetalTexture::~MetalTexture(){
        if(texture != nullptr){
            texture->release();
            texture = nullptr;
        }
    }

    MetalTexture::MetalTexture(MetalTexture&& other)
        : RHITexture(
            other.GetFormat(),
            RHIBarrierSync::None,
            RHIBarrierAccess::NoAccess,
            RHIBarrierLayout::Undefined,
            other.GetMipLevels(),
            other.GetArraySize()
        )
        , texture(other.texture)
    {
        other.texture = nullptr;
    }

    MetalTexture& MetalTexture::operator=(MetalTexture&& other){
        if(texture != nullptr){
            texture->release();
        }

        texture = other.texture;
        other.texture = nullptr;

        return *this;
    }
}
