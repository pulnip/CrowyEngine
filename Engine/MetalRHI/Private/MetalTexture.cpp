#include <Metal/MTLDevice.hpp>
#include "Assert.hpp"
#include "MetalAllocator.hpp"
#include "MetalTexture.hpp"
#include "MetalUtil.hpp"
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    MetalTexture::MetalTexture(
        MetalAllocator& allocator,
        MTL::TextureDescriptor* desc,
        StrView name
    )
        : RHITexture(
            convert(desc->pixelFormat()),
            desc->mipmapLevelCount(),
            desc->arrayLength()
        )
        , allocator(&allocator)
        , allocation(allocator.AllocateTexture(
            desc,
            RHIMemoryType::GPUOnly,
            name
        ))
    {
        texture = NS::RetainPtr(
            static_cast<MTL::Texture*>(allocation.resource)
        );
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

    MetalTexture::~MetalTexture(){
        // this type is movable, and the defaulted move copies both the
        // allocator pointer and the allocation. `texture` is what tells the
        // two apart: NS::SharedPtr nulls the source it was moved out of, so
        // only the object still holding the reference does the freeing
        if(allocator == nullptr || !texture)
            return;

        texture.reset();
        allocator->Free(allocation);
    }

    u64 MetalTexture::getResourceID(const RHITextureViewDesc& view){
        // Unlike D3D12, a Metal resource ID carries the texture's type,
        // so a shader-side texturecube needs an actual cube-typed view;
        const bool wantsCube = std::holds_alternative<
            RHITextureViewDesc::TexCube
        >(view.config);
        const bool wants3D = std::holds_alternative<
            RHITextureViewDesc::Tex3D
        >(view.config);
        const auto wantedType =
            wantsCube ? MTL::TextureTypeCube :
            wants3D   ? MTL::TextureType3D :
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
