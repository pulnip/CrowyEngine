#include <Metal/MTLDevice.hpp>
#include "Assert.hpp"
#include "MetalHeapPool.hpp"
#include "MetalTexture.hpp"
#include "MetalUtil.hpp"
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    MetalTexture::MetalTexture(
        MetalHeapPool& heapPool,
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
        , texture(NS::TransferPtr(heapPool.NewTexture(desc)))
    {
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
            convert(drawable->texture()->pixelFormat()),
            RHIBarrierSync::None,
            RHIBarrierAccess::NoAccess,
            RHIBarrierLayout::Present,
            1,
            1
        )
        , texture(NS::RetainPtr(drawable->texture()))
    {}

    MetalTexture::~MetalTexture() = default;

    u64 MetalTexture::getResourceID(const RHITextureViewDesc&){
        return texture->gpuResourceID()._impl;
    }
}
