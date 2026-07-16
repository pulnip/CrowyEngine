#pragma once

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

        RHIBarrierSync syncState;
        RHIBarrierAccess accessState;
        RHIBarrierLayout layoutState;

    public:
        RHITexture(
            RHIPixelFormat format,
            RHIBarrierSync syncState,
            RHIBarrierAccess accessState,
            RHIBarrierLayout layoutState
        )
            : format(format)
            , syncState(syncState)
            , accessState(accessState)
            , layoutState(layoutState)
        {}
        virtual ~RHITexture() = default;
        CROWY_DECLARE_PINNED(RHITexture)

        RHIPixelFormat GetFormat() const noexcept{
            return format;
        }
        virtual u32 GetWidth() const noexcept = 0;
        virtual u32 GetHeight() const noexcept = 0;
        virtual u16 GetMipLevels() const noexcept = 0;
        // Shader Resource
        virtual u64 GetReadableID() = 0;
        // Unordered Access
        virtual u64 GetWritableID() = 0;

        virtual void* GetNative() noexcept = 0;

        auto GetSyncState() const noexcept{
            return syncState;
        }
        auto TransitionState(RHIBarrierSync newState) noexcept{
            const auto oldState = syncState;
            syncState = newState;
            return oldState;
        }

        auto GetAccessState() const noexcept{
            return accessState;
        }
        auto TransitionState(RHIBarrierAccess newState) noexcept{
            const auto oldState = accessState;
            accessState = newState;
            return oldState;
        }

        auto GetLayoutState() const noexcept{
            return layoutState;
        }
        auto TransitionState(RHIBarrierLayout newState) noexcept{
            const auto oldState = layoutState;
            layoutState = newState;
            return oldState;
        }
    };
}
