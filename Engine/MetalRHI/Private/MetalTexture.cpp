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
            1,
            1
        )
        , texture(NS::RetainPtr(drawable->texture()))
    {}

    MetalTexture::~MetalTexture() = default;

    u64 MetalTexture::getResourceID(const RHITextureViewDesc& view){
        // Unlike D3D12, a Metal resource ID carries the texture's type,
        // so a shader-side texturecube needs an actual cube-typed view;
        const bool wantsCube = std::holds_alternative<
            RHITextureViewDesc::TexCube
        >(view.config);
        const auto wantedType = wantsCube ?
            MTL::TextureTypeCube :
            MTL::TextureType2D;
        const auto format = convert(view.format);

        const bool wholeMipChain =
            view.mostDetailedMip == 0 &&
            view.mipCount == RHI_ALL_MIPS;
        if(wholeMipChain &&
            texture->textureType() == wantedType &&
            texture->pixelFormat() == format
        ){
            return texture->gpuResourceID()._impl;
        }

        if(auto it = views.find(view); it != views.end()){
            return it->second->gpuResourceID()._impl;
        }

        CROWY_ASSERT(view.mostDetailedMip < texture->mipmapLevelCount(),
            "view's most detailed mip out of range"
        );
        const auto mipCount = view.mipCount == RHI_ALL_MIPS ?
            texture->mipmapLevelCount() - view.mostDetailedMip :
            view.mipCount;
        const auto sliceCount = wantsCube ? 6 : 1;

        auto mtlView = NS::TransferPtr(texture->newTextureView(
            format,
            wantedType,
            NS::Range::Make(view.mostDetailedMip, mipCount),
            NS::Range::Make(0, sliceCount)
        ));
        CROWY_ASSERT(mtlView, "failed to create texture view");

        const auto id = mtlView->gpuResourceID()._impl;
        views.emplace(view, std::move(mtlView));

        return id;
    }
}
