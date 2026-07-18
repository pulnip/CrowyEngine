#include "AppFramework.hpp"
#include "RHITexture.hpp"

#if defined(_WIN32)
extern "C" {
    __declspec(dllexport) extern const std::uint32_t D3D12SDKVersion = 619;
    __declspec(dllexport) extern const char* D3D12SDKPath = ".\\";
}
#endif

namespace Crowy
{
    RHIViewport FullViewport(const RHITexture& texture){
        return RHIViewport{
            .x = 0, .y = 0,
            .width = static_cast<f32>(texture.GetWidth()),
            .height = static_cast<f32>(texture.GetHeight()),
            .minDepth = 0, .maxDepth = 1
        };
    }

    RHIScissorRect FullScissorRect(const RHITexture& texture){
        return RHIScissorRect{
            .left = 0, .top = 0,
            .right = static_cast<i32>(texture.GetWidth()),
            .bottom = static_cast<i32>(texture.GetHeight())
        };
    }
}
