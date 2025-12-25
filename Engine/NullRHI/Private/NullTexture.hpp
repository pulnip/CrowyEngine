#pragma once

#include <cstddef>
#include <memory>
#include "RHIAPI.h"
#include "RHIDefinitions.h"
#ifndef USE_STATIC_RHI
    #include "RHITexture.hpp"
#endif

namespace Crowy
{
    class NullTexture
#ifndef USE_STATIC_RHI
        : public RHITexture
#endif
    {
    public:
        NullTexture(const RHITextureCreateDesc& desc)
            : width(desc.width), height(desc.height)
            , format(desc.format)
        {}
        ~NullTexture() = default;

        void uploadData(const void* data,
            uint32_t mipLevel = 0, uint32_t arraySlice = 0
        ) RHI_OVERRIDE{
            // No-Op
        }

        void* getNativeResource() RHI_OVERRIDE{
            return nullptr;
        }

    private:
        size_t width, height;
        RHITextureFormat format = Unknown;
    };
}
