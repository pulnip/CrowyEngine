#include <format>
#include <stdexcept>
#include <utility>
#include "EnumUtil.hpp"
#include "RHIBuffer.hpp"
#include "RHIDevice.hpp"
#include "RHIDefinitions.hpp"
#include "RHITexture.hpp"

namespace Crowy
{
    // each platform should implement this function
#if defined(_WIN32)
    RHIDeviceRAII CreateDX12Device();
#elif defined(__APPLE__)
    RHIDeviceRAII CreateMetalDevice();
#endif

    void RHIDevice::Retire(RHIBufferRAII buffer){
        if(buffer == nullptr)
            return;

        DeferRetire([buffer = std::move(buffer)]{});
    }

    void RHIDevice::Retire(RHITextureRAII texture){
        if(texture == nullptr)
            return;

        DeferRetire([texture = std::move(texture)]{});
    }

    RHIDeviceRAII CreateDevice(RHIBackend backend){
        using enum RHIBackend;

        switch(backend){
    #if defined(_WIN32)
        case DirectX12: return CreateDX12Device();
    #elif defined(__APPLE__)
        case Metal:     return CreateMetalDevice();
    #endif
        default:
            throw std::runtime_error(std::format(
                "Unsupported Backend: {}",
                EnumTraits<RHIBackend>::convert(backend)
            ));
        }
    }
}
