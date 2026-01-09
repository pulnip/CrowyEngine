#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include "semantics.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
    class RenderTargetPool{
    private:
        std::unordered_map<std::string, RHITexturePtr> targets;

    public:
        RenderTargetPool() = default;
        ~RenderTargetPool() = default;
        CROWY_DECLARE_MOVE_ONLY(RenderTargetPool)

        RHITexture* create(
            const std::string& name,
            const RHITextureCreateDesc&,
            RHIDevice&
        );

        inline RHITexture* get(const std::string& name){
            return const_cast<RHITexture*>(
                static_cast<const RenderTargetPool*>(this)->get(name)
            );
        }
        const RHITexture* get(const std::string& name) const;

        // TODO. clean-up transient resource
        // void endFrame();
    };
}