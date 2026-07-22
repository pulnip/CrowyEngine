#pragma once

#include <algorithm>
#include <vector>
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
        // [ arraySlice 0 = [mipLevel 0, mipLevel 1, ...],
        //   arraySlice 1 = [...],
        //   ...
        // ] flattened as [arraySlice * mipLevels + mipLevel]
        std::vector<RHIBarrierPoint> barriers;

    public:
        RHITexture(
            RHIPixelFormat format,
            RHIBarrierSync syncState,
            RHIBarrierAccess accessState,
            RHIBarrierLayout layoutState,
            u32 mipLevels = 1,
            u32 arraySize = 1
        )
            : format(format)
            , mipLevels(mipLevels)
            , barriers(
                static_cast<usize>(mipLevels) * arraySize,
                RHIBarrierPoint{
                    .sync = syncState,
                    .access = accessState,
                    .layout = layoutState
                }
            )
        {
            CROWY_ASSERT(mipLevels > 0, "texture needs at least one mip level");
            CROWY_ASSERT(arraySize > 0, "texture needs at least one array slice");
        }
        virtual ~RHITexture() = default;
        CROWY_DECLARE_PINNED(RHITexture)

        RHIPixelFormat GetFormat() const noexcept{
            return format;
        }
        // mip 0
        virtual u32 GetWidth() const noexcept = 0;
        virtual u32 GetHeight() const noexcept = 0;
        // a mip never shrinks below one texel, so this is not a plain shift.
        u32 GetWidth(u32 mipLevel) const noexcept{
            return MipExtent(GetWidth(), mipLevel);
        }
        u32 GetHeight(u32 mipLevel) const noexcept{
            return MipExtent(GetHeight(), mipLevel);
        }
        u32 GetMipLevels() const noexcept{
            return mipLevels;
        }
        u32 GetArraySize() const noexcept{
            return static_cast<u32>(barriers.size()) / mipLevels;
        }
        // Shader Resource
        virtual u64 GetReadableID(const RHITextureViewDesc&) = 0;
        // Unordered Access
        virtual u64 GetWritableID(const RHITextureViewDesc&) = 0;

        u64 GetReadableID(){
            return GetReadableID(RHITextureViewDesc{
                .format = GetFormat(),
                .config = RHITextureViewDesc::Tex2D{}
            });
        }
        u64 GetWritableID(){
            return GetWritableID(RHITextureViewDesc{
                .format = GetFormat(),
                .config = RHITextureViewDesc::Tex2D{}
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

        // before-state in range must be equal
        RHIBarrierPoint TransitionState(
            const RHIBarrierPoint& after,
            const RHISubresourceRange& range = {}
        ){
            const auto resolved = ResolveRange(range);
            CROWY_ASSERT(
                resolved.firstMip + resolved.numMips <= GetMipLevels(),
                "mip range out of bounds"
            );
            CROWY_ASSERT(
                resolved.firstArraySlice + resolved.numArraySlice
                    <= GetArraySize(),
                "array slice range out of bounds"
            );
            CROWY_ASSERT(resolved.numMips > 0 && resolved.numArraySlice > 0,
                "empty subresource range"
            );

            const RHIBarrierPoint before = barriers[SubresourceIndex(
                resolved.firstMip,
                resolved.firstArraySlice
            )];

            for(u32 slice=0; slice<resolved.numArraySlice; ++slice){
                // flattened, so the mips of one slice are contiguous
                const auto base = SubresourceIndex(
                    resolved.firstMip,
                    resolved.firstArraySlice + slice
                );
                for(u32 mip=0; mip<resolved.numMips; ++mip){
                    auto& barrier = barriers[base + mip];
                    CROWY_ASSERT(barrier == before,
                        "subresources of one barrier are in different states"
                    );
                    barrier = after;
                }
            }

            return before;
        }

        RHIBarrierSync GetSyncState(
            u32 mipLevel = 0,
            u32 arraySlice = 0
        ) const{
            auto& barrier = barriers[SubresourceIndex(mipLevel, arraySlice)];
            return barrier.sync;
        }
        RHIBarrierSync TransitionState(
            RHIBarrierSync newState,
            u32 mipLevel = 0,
            u32 arraySlice = 0
        ){
            auto& barrier = barriers[SubresourceIndex(mipLevel, arraySlice)];
            const auto oldState = barrier.sync;
            barrier.sync = newState;
            return oldState;
        }

        RHIBarrierAccess GetAccessState(
            u32 mipLevel = 0,
            u32 arraySlice = 0
        ) const{
            auto& barrier = barriers[SubresourceIndex(mipLevel, arraySlice)];
            return barrier.access;
        }
        RHIBarrierAccess TransitionState(
            RHIBarrierAccess newState,
            u32 mipLevel = 0,
            u32 arraySlice = 0
        ){
            auto& barrier = barriers[SubresourceIndex(mipLevel, arraySlice)];
            const auto oldState = barrier.access;
            barrier.access = newState;
            return oldState;
        }

        RHIBarrierLayout GetLayoutState(
            u32 mipLevel = 0,
            u32 arraySlice = 0
        ) const{
            auto& barrier = barriers[SubresourceIndex(mipLevel, arraySlice)];
            return barrier.layout;
        }
        RHIBarrierLayout TransitionState(
            RHIBarrierLayout newState,
            u32 mipLevel = 0,
            u32 arraySlice = 0
        ){
            auto& barrier = barriers[SubresourceIndex(mipLevel, arraySlice)];
            const auto oldState = barrier.layout;
            barrier.layout = newState;
            return oldState;
        }

    private:
        u32 MipExtent(u32 extent, u32 mipLevel) const noexcept{
            CROWY_ASSERT(mipLevel < mipLevels, "mip level out of range");

            // a shift wider than the type is undefined, and the assert above is
            // gone in release builds
            if(mipLevel >= 32){
                return 1;
            }
            return std::max(1u, extent >> mipLevel);
        }

        usize SubresourceIndex(u32 mipLevel, u32 arraySlice) const{
            CROWY_ASSERT(mipLevel < mipLevels, "mip level out of range");

            const usize index = static_cast<usize>(arraySlice) * mipLevels + mipLevel;
            CROWY_ASSERT(index < barriers.size(), "array slice out of range");

            return index;
        }
    };
}
