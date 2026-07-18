#pragma once

#include <type_traits>
#include "Semantics.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    class RHIBuffer{
    public:
        CROWY_DECLARE_INTERFACE(RHIBuffer)

        // Notice! only valid for upload buffer
        virtual void Upload(
            const void* data, u32 size,
            u32 offset = 0
        ) = 0;
        virtual void UploadAll(
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
        virtual u64 GetReadableID(const RHIBufferViewDesc&) = 0;
        u64 GetReadableID(){
            return GetReadableID(RHIBufferViewDesc{
                .offset = 0,
                .size = GetSize(),
                .config = RHIBufferViewDesc::Raw{}
            });
        }
        u64 GetReadableID(RHIPixelFormat format){
            return GetReadableID(RHIBufferViewDesc{
                .offset = 0,
                .size = GetSize(),
                .config = RHIBufferViewDesc::Typed{
                    .format = format
                }
            });
        }
        u64 GetReadableID(u32 stride){
            return GetReadableID(RHIBufferViewDesc{
                .offset = 0,
                .size = GetSize(),
                .config = RHIBufferViewDesc::Structured{
                    .stride = stride
                }
            });
        }
        // Unordered Access
        virtual u64 GetWritableID(const RHIBufferViewDesc&) = 0;
        u64 GetWritableID(){
            return GetWritableID(RHIBufferViewDesc{
                .offset = 0,
                .size = GetSize(),
                .config = RHIBufferViewDesc::Raw{}
            });
        }
        u64 GetWritableID(RHIPixelFormat format){
            return GetWritableID(RHIBufferViewDesc{
                .offset = 0,
                .size = GetSize(),
                .config = RHIBufferViewDesc::Typed{
                    .format = format
                }
            });
        }
        u64 GetWritableID(u32 stride){
            return GetWritableID(RHIBufferViewDesc{
                .offset = 0,
                .size = GetSize(),
                .config = RHIBufferViewDesc::Structured{
                    .stride = stride
                }
            });
        }

        virtual RHIBarrierSync GetSyncState() const = 0;
        virtual RHIBarrierSync TransitionState(RHIBarrierSync newState) = 0;
        virtual RHIBarrierAccess GetAccessState() const = 0;
        virtual RHIBarrierAccess TransitionState(RHIBarrierAccess newState) = 0;
    };
}
