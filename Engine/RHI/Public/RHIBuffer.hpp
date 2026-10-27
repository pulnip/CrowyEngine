#pragma once

#include <cstring>
#include <type_traits>

#include "Assert.hpp"
#include "PtrUtil.hpp"
#include "RHIDefinitions.hpp"
#include "Semantics.hpp"
#include "VariantUtil.hpp"

namespace Crowy
{
    class RHIBuffer {
    public:
        CROWY_DECLARE_INTERFACE(RHIBuffer)

        void Upload(const void* data, u32 size, u32 offset = 0) {
            std::memcpy(mappedRange(size, offset), data, size);
        }

        template<typename T>
            requires(!std::is_pointer_v<T> && std::is_trivially_copyable_v<T>)
        void Upload(const T& data, u32 offset = 0) {
            Upload(&data, sizeof(T), offset);
        }

        void Download(void* data, u32 size, u32 offset = 0) {
            std::memcpy(data, mappedRange(size, offset), size);
        }

        template<typename T>
            requires(!std::is_pointer_v<T> && std::is_trivially_copyable_v<T>)
        void Download(T& data, u32 offset = 0) {
            Download(&data, sizeof(T), offset);
        }

        virtual u32 GetSize() const noexcept = 0;

        // Persistently mapped storage, or null when the CPU cannot reach it.
        // Feeds RHIBufferSlice::cpuPtr; prefer Upload/Download elsewhere.
        virtual void* GetMappedPtr() noexcept = 0;

        u64 GetReadableID(const RHIBufferViewDesc& view) {
            checkView(view);

            return readableIDOf(view);
        }
        u64 GetReadableID() {
            return GetReadableID(wholeBuffer(RHIBufferViewDesc::Raw{}));
        }
        u64 GetReadableID(RHIPixelFormat format) {
            return GetReadableID(
                wholeBuffer(RHIBufferViewDesc::Typed{.format = format})
            );
        }
        u64 GetReadableID(u32 stride) {
            return GetReadableID(
                wholeBuffer(RHIBufferViewDesc::Structured{.stride = stride})
            );
        }
        // Unordered Access
        u64 GetWritableID(const RHIBufferViewDesc& view) {
            checkView(view);

            return writableIDOf(view);
        }
        u64 GetWritableID() {
            return GetWritableID(wholeBuffer(RHIBufferViewDesc::Raw{}));
        }
        u64 GetWritableID(RHIPixelFormat format) {
            return GetWritableID(
                wholeBuffer(RHIBufferViewDesc::Typed{.format = format})
            );
        }
        u64 GetWritableID(u32 stride) {
            return GetWritableID(
                wholeBuffer(RHIBufferViewDesc::Structured{.stride = stride})
            );
        }

    private:
        virtual u64 readableIDOf(const RHIBufferViewDesc&) = 0;
        virtual u64 writableIDOf(const RHIBufferViewDesc&) = 0;

        RHIBufferViewDesc wholeBuffer(RHIBufferViewDesc::Config config) const {
            return RHIBufferViewDesc{
                .offset = 0,
                .size = GetSize(),
                .config = config
            };
        }

        void* mappedRange(u32 size, u32 offset) {
            auto* mapped = GetMappedPtr();
            CROWY_ASSERT(mapped != nullptr, "the CPU cannot reach this buffer");
            CROWY_ASSERT(
                offset <= GetSize() && size <= GetSize() - offset,
                "[{}, {}) runs past the {}-byte buffer",
                offset,
                u64{offset} + size,
                GetSize()
            );

            return ptrAdd(mapped, offset);
        }

        void checkView(const RHIBufferViewDesc& view) const {
            const auto elementSize = std::visit(
                overload{
                    [](const RHIBufferViewDesc::Raw&) { return 4u; },
                    [](const RHIBufferViewDesc::Typed& typed) {
                        return detail::getBytesPerPixel(typed.format);
                    },
                    [](const RHIBufferViewDesc::Structured& structured) {
                        return structured.stride;
                    }
                },
                view.config
            );
            CROWY_ASSERT(
                elementSize > 0,
                "a buffer view needs an element size"
            );
            CROWY_ASSERT(
                view.offset % elementSize == 0,
                "buffer view offset {} does not land on a {}-byte element",
                view.offset,
                elementSize
            );
            CROWY_ASSERT(
                view.offset <= GetSize() &&
                    view.size <= GetSize() - view.offset,
                "buffer view [{}, {}) runs past the {}-byte buffer",
                view.offset,
                u64{view.offset} + view.size,
                GetSize()
            );
        }
    };
}
