#include <CoreGraphics/CGGeometry.h>
#include <QuartzCore/CAMetalLayer.hpp>
#include "Assert.hpp"
#include "MetalCommandList.hpp"
#include "MetalSwapchain.hpp"
#include "MetalTexture.hpp"
#include "MetalUtil.hpp"

namespace Crowy
{
    MetalSwapchain::MetalSwapchain(
        MTL::Device& device,
        const RHISwapchainCreateDesc& desc
    )
        : RHISwapchain(desc.bufferDesc.format)
        , view(SDL_Metal_CreateView(static_cast<SDL_Window*>(desc.sdlWindow)))
        , metalLayer(static_cast<CA::MetalLayer*>(SDL_Metal_GetLayer(view)))
    {
        CROWY_ASSERT(metalLayer != nullptr);

        metalLayer->setDevice(&device);
        metalLayer->setPixelFormat(convert(desc.bufferDesc.format));
        metalLayer->setFramebufferOnly(false);
        metalLayer->setDrawableSize(CGSizeMake(desc.bufferDesc.width, desc.bufferDesc.height));

        // NOTE. discard desc.debugName, desc.vsync
    }

    MetalSwapchain::~MetalSwapchain(){
        currentDrawable = nullptr;
    }

    bool MetalSwapchain::AcquireNextImage() noexcept{
        currentDrawable = metalLayer->nextDrawable();
        *backBuffer = MetalTexture(currentDrawable);

        return currentDrawable != nullptr;
    }

    void MetalSwapchain::Resize(u32 newWidth, u32 newHeight){
        metalLayer->setDrawableSize(CGSizeMake(newWidth, newHeight));
        currentDrawable = nullptr;
        backBuffer = nullptr;
    }

    void MetalSwapchain::Present(MetalCommandList& cmdList){
        auto cmdBuffer = cmdList.Get();
        cmdBuffer->presentDrawable(currentDrawable);
    }
}
