#pragma once

#include <algorithm>
#include "Assert.hpp"
#include "Semantics.hpp"
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    // GPU-only resource
    class RHITexture{
    private:
        // Requested format; Could be differ from Actual format
        RHIPixelFormat format = RHIPixelFormat::Unknown;

        u32 mipLevels = 1;
        u32 arraySize = 1;

    public:
        RHITexture() = default;

        RHITexture(
            RHIPixelFormat format,
            u32 mipLevels = 1,
            u32 arraySize = 1
        )
            : format(format)
            , mipLevels(mipLevels)
            , arraySize(arraySize)
        {
            CROWY_ASSERT(mipLevels > 0, "texture needs at least one mip level");
            CROWY_ASSERT(arraySize > 0, "texture needs at least one array slice");
        }
        virtual ~RHITexture() = default;
        CROWY_DECLARE_MOVE_ONLY_NOEXCEPT(RHITexture)

        RHIPixelFormat GetFormat() const noexcept{
            return format;
        }
        // mip 0
        virtual u32 GetWidth() const noexcept = 0;
        virtual u32 GetHeight() const noexcept = 0;
        // 1 unless the texture is 3D
        virtual u32 GetDepth() const noexcept = 0;
        // a mip never shrinks below one texel, so this is not a plain shift.
        u32 GetWidth(u32 mipLevel) const noexcept{
            return MipExtent(GetWidth(), mipLevel);
        }
        u32 GetHeight(u32 mipLevel) const noexcept{
            return MipExtent(GetHeight(), mipLevel);
        }
        u32 GetDepth(u32 mipLevel) const noexcept{
            return MipExtent(GetDepth(), mipLevel);
        }
        u32 GetMipLevels() const noexcept{
            return mipLevels;
        }
        u32 GetArraySize() const noexcept{
            return arraySize;
        }
        // Shader Resource
        virtual u64 GetReadableID(const RHITextureViewDesc&) = 0;
        // Unordered Access
        virtual u64 GetWritableID(const RHITextureViewDesc&) = 0;

        u64 GetReadableID(){
            return GetReadableID(RHITextureViewDesc{
                .format = GetFormat(),
                .config = DefaultViewConfig()
            });
        }
        u64 GetWritableID(){
            return GetWritableID(RHITextureViewDesc{
                .format = GetFormat(),
                .config = DefaultViewConfig()
            });
        }

        virtual void* GetNative() noexcept = 0;

        // expands the "whole resource" sentinel into concrete counts. backends
        // must go through this so they all agree on what a default range means.
        RHISubresourceRange ResolveRange(const RHISubresourceRange& range) const{
            if(range.numMips != 0){
                return range;
            }

            CROWY_ASSERT(range.firstMip == RHI_ALL_SUBRESOURCES,
                "a flat subresource index is not a range; "
                "give explicit mip and array slice counts"
            );
            return RHISubresourceRange{
                .firstMip = 0,
                .numMips = GetMipLevels(),
                .firstArraySlice = 0,
                .numArraySlice = GetArraySize()
            };
        }

    private:
        // a 3D texture's whole-resource view must be 3D-typed
        RHITextureViewDesc::Config DefaultViewConfig() const noexcept{
            return GetDepth() > 1 ?
                RHITextureViewDesc::Config{RHITextureViewDesc::Tex3D{}} :
                RHITextureViewDesc::Config{RHITextureViewDesc::Tex2D{}};
        }

        u32 MipExtent(u32 extent, u32 mipLevel) const noexcept{
            CROWY_ASSERT(mipLevel < mipLevels, "mip level out of range");

            // a shift wider than the type is undefined, and the assert above is
            // gone in release builds
            if(mipLevel >= 32){
                return 1;
            }
            return std::max(1u, extent >> mipLevel);
        }
    };
}
