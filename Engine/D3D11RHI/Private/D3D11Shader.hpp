#pragma once

#include <cstddef>
#include <memory>
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHIShader.hpp"
#endif

namespace Crowy
{
    class D3D11Shader
#ifndef USE_STATIC_RHI
        : public RHIShader
#endif
    {
    private:
        const RHIShaderStage stage;

    public:
        D3D11Shader(const RHIShaderCreateDesc& desc)
            : stage(desc.stage){}
        ~D3D11Shader() = default;

        RHIShaderStage getStage() const noexcept RHI_OVERRIDE{
            return stage;
        }
    };
}
