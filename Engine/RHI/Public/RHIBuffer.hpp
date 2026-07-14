#pragma once

#include <type_traits>
#include "Semantics.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    class RHIBuffer{
    public:
        SMOL_DECLARE_INTERFACE(RHIBuffer)

        // Notice! only valid for upload buffer
        virtual void Upload(
            const void* data, u32 size,
            u32 offset = 0
        ) = 0;

        // type-safe helper
        template<typename T>
            requires (!std::is_pointer_v<T> && std::is_trivially_copyable_v<T>)
        void Upload(const T& data, u32 offset = 0){
            Upload(&data, sizeof(T), offset);
        }

        // Notice! only valid for readback buffer (performance issue)
        virtual void Download(
            void* data, u32 size,
            u32 offset = 0
        ) = 0;

        // type-safe helper
        template<typename T>
            requires (!std::is_pointer_v<T> && std::is_trivially_copyable_v<T>)
        void Download(T& data, u32 offset = 0){
            Download(&data, sizeof(T), offset);
        }

        virtual u32 GetSize() const noexcept = 0;
        // Shader Resource
        virtual u64 GetReadableID() = 0;
        // Unordered Access
        virtual u64 GetWritableID() = 0;

        virtual RHIBarrierSync GetSyncState() const = 0;
        virtual RHIBarrierSync TransitionState(RHIBarrierSync newState) = 0;
        virtual RHIBarrierAccess GetAccessState() const = 0;
        virtual RHIBarrierAccess TransitionState(RHIBarrierAccess newState) = 0;
    };
}
