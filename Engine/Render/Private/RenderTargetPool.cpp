#include "RenderTargetPool.hpp"
#include "RHIDevice.hpp"
#include "RHITexture.hpp"

namespace Crowy
{
    RHITexture* RenderTargetPool::create(
        const std::string& name,
        const RHITextureCreateDesc& desc,
        RHIDevice& device
    ){
        auto it = targets.find(name);
        if(it != targets.end()){
            return it->second.get();
        }

        auto newTexture = device.createTexture(desc);
        auto ref = newTexture.get();
        targets.emplace(name, std::move(newTexture));

        return ref;
    }

    const RHITexture* RenderTargetPool::get(const std::string& name) const{
        if(auto it = targets.find(name); it != targets.end()){
            return it->second.get();
        }
        return nullptr;
    }
}