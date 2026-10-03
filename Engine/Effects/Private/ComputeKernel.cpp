#include "ComputeKernel.hpp"

#include <utility>

#include "RHIDevice.hpp"
#include "RHIPipelineState.hpp"

namespace Crowy
{
    ComputeKernel::~ComputeKernel() = default;

    ComputeKernel::ComputeKernel(RHIDevice& device, RHIShaderDesc shader)
        : shader(std::move(shader)),
          pipeline(device.CreatePipelineState(
              RHIComputePipelineStateDesc{.computeShader = this->shader},
              this->shader.entryPoint
          )) {}
}
