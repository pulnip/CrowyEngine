#include <Metal/Metal.hpp>
#include "EnumUtil.hpp"
#include "MetalTexture.hpp"
#include "MetalUtil.hpp"
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"

namespace{
    auto convert(Crowy::RHITextureUsage usage){
        using namespace Crowy;
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
}

namespace Crowy
{
    MetalTexture::MetalTexture(
        MTL::Device& device,
        const RHITextureCreateDesc& desc,
        StrView name
    )
        : currentState(desc.initialState)
    {
        auto texDesc = MTL::TextureDescriptor::alloc()->init();
        texDesc->setWidth(desc.width);
        texDesc->setHeight(desc.height);
        texDesc->setDepth(desc.depth);
        texDesc->setMipmapLevelCount(desc.mipLevels);
        texDesc->setArrayLength(desc.arraySize);

        texDesc->setPixelFormat(convert(desc.format));
        texDesc->setTextureType(
            desc.depth > 1 ? MTL::TextureType3D :
                (desc.arraySize > 1 ? MTL::TextureType2DArray
                                    : MTL::TextureType2D)
        );
        texDesc->setUsage(::convert(desc.usage));
    #if TARGET_OS_OSX
        bool needsGPUOnly = hasFlag(desc.access, RHIMemoryAccess::GPUOnly);
        texDesc->setStorageMode(needsGPUOnly ?
            MTL::StorageModePrivate : MTL::StorageModeShared);
    #else
        texDesc->setStorageMode(MTL::StorageModeShared);
    #endif

        texture = device.newTexture(texDesc);
        texDesc->release();

        if(desc.initialData != nullptr){
            auto bytesPerPixel = getBytesPerPixel(desc.format);
            auto bytesPerRow = desc.width * bytesPerPixel;
            auto region = MTL::Region::Make2D(
                0,
                0,
                desc.width,
                desc.height
            );

            texture->replaceRegion(
                region,
                0,
                0,
                desc.initialData,
                bytesPerRow,
                0
            );
        }

    #if defined(_DEBUG) || !defined(NDEBUG)
        if(!name.empty()){
            texture->setLabel(
                NS::String::string(name.data(), NS::UTF8StringEncoding)
            );
        }
    #endif
    }

    MetalTexture::MetalTexture(
        CA::MetalDrawable* drawable
    ){
        texture = drawable->texture();
        texture->retain();
    }

    MetalTexture::~MetalTexture(){
        texture->release();
    }
}
