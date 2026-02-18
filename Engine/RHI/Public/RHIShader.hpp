#pragma once

#include <memory>
#include "semantics.hpp"
#include "RHIDefinitions.hpp"

#ifdef USE_STATIC_RHI
    #ifdef USE_METAL_BACKEND
        #include "MetalShader.hpp"
    #else
        #include "NullShader.hpp"
    #endif
#endif

namespace Crowy
{
#ifdef USE_STATIC_RHI
    template<typename T>
    concept RHIShaderType = requires(T shader){
    };
    static_assert(RHIShaderType<RHIShader>);
#else
    class RHIShader{
    public:
        CROWY_DECLARE_INTERFACE_NOEXCEPT(RHIShader)

        virtual RHIShaderStage getStage() const noexcept = 0;
    };
#endif

    using RHIShaderPtr = std::unique_ptr<RHIShader>;
}
