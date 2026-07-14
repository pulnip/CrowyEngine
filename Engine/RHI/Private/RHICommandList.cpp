#include "RHICommandList.hpp"
#include "RHISwapchain.hpp"

namespace Crowy
{
    void RHICommandList::Copy(
        RHITexture& src,
        RHISwapchain& dst
    ){
        auto& backBuffer = dst.GetCurrentTexture();
        Copy(
            src,
            backBuffer
        );
    }
}
