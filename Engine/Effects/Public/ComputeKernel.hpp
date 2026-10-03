#pragma once

#include "Primitives.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    // one compute entry and its pipeline
    class ComputeKernel {
    private:
        RHIShaderDesc shader;
        RHIComputePipelineStateRAII pipeline;

    public:
        ~ComputeKernel();
        CROWY_DECLARE_PINNED(ComputeKernel)

        ComputeKernel(RHIDevice& device, RHIShaderDesc shader);

        const RHIShaderDesc& Shader() const noexcept { return shader; }
        RHIComputePipelineState& Pipeline() noexcept { return *pipeline; }
    };
}
