#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
    class RHIDevice;

    class RenderTargetPool{
    private:
        RHIDevice* device;

        std::unordered_map<std::string, RHITexturePtr> targets;

    public:
        RenderTargetPool(RHIDevice&);

        RHITexture* acquire(
            const std::string& name,
            uint32_t width,
            uint32_t height,
            RHITextureFormat format
        );

        inline RHITexture* get(const std::string& name){
            return const_cast<RHITexture*>(
                static_cast<const RenderTargetPool*>(this)->get(name)
            );
        }
        const RHITexture* get(const std::string& name) const;

        void onResize(uint32_t newWidth, uint32_t newHeight);

        // clean-up transient resource
        void endFrame();
    };
}