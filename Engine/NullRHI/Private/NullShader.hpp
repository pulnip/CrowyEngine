#pragma once

#include <cstddef>
#include <memory>
#include "RHIAPI.h"
#include "RHIDefinitions.h"
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
    public:
        NullShader(const RHIShaderCreateDesc& desc)
            : stage(desc.stage){}
        ~NullShader() = default;

        RHIShaderStage getStage() const RHI_OVERRIDE{
            return stage;
        }

    private:
        const RHIShaderStage stage;
    };
}
