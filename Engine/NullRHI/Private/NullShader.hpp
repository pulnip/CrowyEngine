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
    class NullShader
#ifndef USE_STATIC_RHI
        : public RHIShader
#endif
    {
    private:
        const RHIShaderStage stage;

    public:
        NullShader(const RHIShaderCreateDesc& desc)
            : stage(desc.stage){}
        ~NullShader() = default;

        RHIShaderStage getStage() const noexcept RHI_OVERRIDE{
            return stage;
        }
    };
}
