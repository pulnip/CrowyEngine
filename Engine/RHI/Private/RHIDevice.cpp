#include <format>
#include <stdexcept>
#include "EnumUtil.hpp"
#include "RHIDevice.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    // each platform should implement this function
#if defined(_WIN32)
    RHIDeviceRAII CreateDX12Device();
#elif defined(__APPLE__)
    RHIDeviceRAII CreateMetalDevice();
#endif

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
