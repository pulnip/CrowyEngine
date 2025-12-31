#include "RenderTargetPool.hpp"
#include "RHIDevice.hpp"
#include "RHITexture.hpp"

namespace Crowy
{
    RenderTargetPool::RenderTargetPool(RHIDevice& device)
        :device(&device){}

    RHITexture* RenderTargetPool::acquire(
        const std::string& name,
        uint32_t width,
        uint32_t height,
        RHITextureFormat format
    ){
        auto it = targets.find(name);
        if(it != targets.end()){
            return it->second.get();
        }

        auto newTexture = device->createTexture(
            RHITextureCreateDesc{
                .width = width,
                .height = height,
                .format = format,
            }
        );
        auto ref = newTexture.get();
        targets.emplace(name, std::move(newTexture));

        return ref;
    }
}