#include <utility>
#include <CoreGraphics/CGGeometry.h>
#include <QuartzCore/CAMetalLayer.hpp>
#include <Metal/MTLCommandBuffer.hpp>
#include "Assert.hpp"
#include "MetalFrameDump.hpp"
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
        // readable drawables; DumpFrame depends on this
        metalLayer->setFramebufferOnly(false);
        metalLayer->setDrawableSize(CGSizeMake(
            desc.bufferDesc.width,
            desc.bufferDesc.height
        ));

        // NOTE. discard desc.debugName, desc.vsync
    }

    MetalSwapchain::~MetalSwapchain(){
        SDL_Metal_DestroyView(view);
        currentDrawable = nullptr;
    }

    bool MetalSwapchain::AcquireNextImage() noexcept{
        currentDrawable = metalLayer->nextDrawable();
        backBuffer = currentDrawable != nullptr ?
            MetalTexture(currentDrawable) :
            MetalTexture{};

        return currentDrawable != nullptr;
    }

    void MetalSwapchain::Resize(u32 newWidth, u32 newHeight){
        metalLayer->setDrawableSize(CGSizeMake(newWidth, newHeight));
        currentDrawable = nullptr;
        backBuffer = MetalTexture{};
    }

    void MetalSwapchain::Present(MTL::CommandBuffer& cmdBuffer, u64 frame){
        // a frame without a drawable is not a presented frame
        if(currentDrawable != nullptr){
            while(auto job = TakeFrameDump(frame)){
                DumpFrame(
                    cmdBuffer,
                    *currentDrawable,
                    std::move(job->path),
                    std::move(job->completion)
                );
            }
        }
        cmdBuffer.presentDrawable(currentDrawable);
    }
}
