#include <format>
#include <stdexcept>

#include "EnumUtil.hpp"
#include "RHIDefinitions.hpp"
#include "RHIDevice.hpp"

// Apart from RHIDevice.cpp, so code that only uses the device's helpers
// links without a backend, as a device-less test does
namespace Crowy
{
    // each platform should implement this function
#if defined(_WIN32)
    RHIDeviceRAII createDX12Device();
#elif defined(__APPLE__)
    RHIDeviceRAII createMetalDevice();
#endif

    RHIDeviceRAII createDevice(RHIBackend backend) {
        using enum RHIBackend;

        switch(backend) {
#if defined(_WIN32)
        case DirectX12:
            return createDX12Device();
#elif defined(__APPLE__)
        case Metal:
            return createMetalDevice();
#endif
        default:
            throw std::runtime_error(
                std::format("Unsupported Backend: {}", enumName(backend))
            );
        }
    }
}
