#pragma once

#include <cstddef>
#include <Metal/Metal.hpp>
#include "MetalUtil.hpp"
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHITexture.hpp"
#endif

namespace Crowy
{
    inline auto convert(RHITextureUsage usage){
        MTL::TextureUsage mtlUsage = 0;

        if(has_flag(usage, RHITextureUsage::ShaderResource))
            mtlUsage |= MTL::TextureUsageShaderRead;
        if(has_flag(usage, RHITextureUsage::RenderTarget))
            mtlUsage |= MTL::TextureUsageRenderTarget;
        if(has_flag(usage, RHITextureUsage::DepthStencil))
            mtlUsage |= MTL::TextureUsageRenderTarget;
        if(has_flag(usage, RHITextureUsage::UnorderedAccess))
            mtlUsage |= MTL::TextureUsageShaderWrite;

        return mtlUsage;
    }

    class MetalTexture
#ifndef USE_STATIC_RHI
        : public RHITexture
#endif
    {
    private:
        MTL::Texture* texture;
        size_t width, height;
        RHITextureFormat format = RHITextureFormat::Unknown;
        RHIResourceState currentState = RHIResourceState::Common;

    public:
        MetalTexture(
            MTL::Device* device,
            const RHITextureCreateDesc& desc
        ) noexcept
            : width(desc.width), height(desc.height)
            , format(desc.format)
            , currentState(desc.initialState)
        {
            auto texDesc = MTL::TextureDescriptor::alloc()->init();
            texDesc->setWidth(desc.width);
            texDesc->setHeight(desc.height);
            texDesc->setDepth(desc.depth);
            texDesc->setMipmapLevelCount(desc.mipLevels);
            texDesc->setArrayLength(desc.arraySize);

            texDesc->setPixelFormat(convertTextureFormat(desc.format));
            texDesc->setTextureType(
                desc.depth > 1 ? MTL::TextureType3D :
                    (desc.arraySize > 1 ? MTL::TextureType2DArray
                                        : MTL::TextureType2D)
            );
            texDesc->setUsage(convert(desc.usage));
        #if TARGET_OS_OSX                                                             
            bool needsGPUOnly = has_flag(desc.usage, RHITextureUsage::RenderTarget) || 
                                has_flag(desc.usage, RHITextureUsage::DepthStencil);   
            texDesc->setStorageMode(needsGPUOnly ?
                MTL::StorageModePrivate : MTL::StorageModeShared);           
        #else                                                                         
            texDesc->setStorageMode(MTL::StorageModeShared);                          
        #endif

            texture = device->newTexture(texDesc);
            texDesc->release();

        #if defined(_DEBUG) || !defined(NDEBUG)
            if(!desc.debugName.empty()){
                texture->setLabel(
                    NS::String::string(desc.debugName.c_str(), NS::UTF8StringEncoding)
                );
            }
        #endif

            if(desc.initialData)
                upload(desc.initialData);
        }
        ~MetalTexture(){
            texture->release();
        }

        void upload(const void* data,
            uint32_t mipLevel = 0, uint32_t arraySlice = 0
        ) noexcept RHI_OVERRIDE{
            auto bytesPerPixel = getBytesPerPixel(format);
            auto bytesPerRow = width * bytesPerPixel;
            auto region = MTL::Region::Make2D(0, 0, width, height);

            texture->replaceRegion(
                region,
                mipLevel,
                arraySlice,
                data,
                bytesPerRow,
                0
            );
        }

        MTL::Texture* get() const{ return texture; }

        RHITextureFormat getFormat() const noexcept RHI_OVERRIDE{
            return format;
        }
        size_t getWidth() const noexcept RHI_OVERRIDE{
            return width;
        }
        size_t getHeight() const noexcept RHI_OVERRIDE{
            return height;
        }

        RHIResourceState getState() const noexcept RHI_OVERRIDE{
            return currentState;
        }

        void setState(RHIResourceState state) noexcept RHI_OVERRIDE{
            currentState = state;
        }
    };
}
