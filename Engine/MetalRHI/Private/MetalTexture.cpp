#include <Metal/MTLDevice.hpp>
#include "MetalTexture.hpp"
#include "MetalUtil.hpp"
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    MetalTexture::MetalTexture(
        MTL::Device& device,
        MTL::TextureDescriptor* desc,
        StrView name
    )
        : RHITexture(
            convert(desc->pixelFormat()),
            RHIBarrierSync::None,
            RHIBarrierAccess::NoAccess,
            RHIBarrierLayout::Undefined,
            desc->mipmapLevelCount(),
            desc->arrayLength()
        )
    {
        texture = device.newTexture(desc);

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

    u64 MetalTexture::getResourceID(const RHITextureViewDesc&){
        return texture->gpuResourceID()._impl;
    }
}
