#include <utility>
#include "EnumUtil.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    template<>
    CStr EnumTraits<RHIBackend>::name = "RHIBackend";

    template<>
    CStr EnumTraits<RHIBackend>::convert(RHIBackend e){
        using enum RHIBackend;

        switch(e){
        case DirectX12: return "DirectX12";
        case Metal:     return "Metal";
        case WebGPU:    return "WebGPU";
        default:
            std::unreachable();
        }
    }
}
