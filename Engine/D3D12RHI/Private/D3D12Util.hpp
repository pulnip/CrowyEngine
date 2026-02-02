#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include "RHIDefinitions.hpp"

namespace Crowy
{
    DXGI_FORMAT convert(RHITextureFormat, bool isShaderResource=true, bool isDepthTarget=false);
    D3D12_COMPARISON_FUNC convert(RHIComparisonFunc);
    D3D12_RESOURCE_STATES convert(RHIResourceState);
}