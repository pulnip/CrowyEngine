#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include "RHIDefinitions.hpp"

namespace Crowy
{
    DXGI_FORMAT convertTextureFormat(RHITextureFormat format);
    D3D12_RESOURCE_STATES convertResourceState(RHIResourceState state);
    D3D12_COMPARISON_FUNC convertComparisonFunc(RHIComparisonFunc func);
    D3D12_BLEND convertBlend(RHIBlend blend);
    D3D12_BLEND_OP convertBlendOp(RHIBlendOp op);
    D3D12_CULL_MODE convertCullMode(RHICullMode mode);
    D3D12_FILL_MODE convertFillMode(RHIFillMode mode);
    D3D12_PRIMITIVE_TOPOLOGY_TYPE convertTopologyType(RHIPrimitiveTopology topology);
    D3D_PRIMITIVE_TOPOLOGY convertTopology(RHIPrimitiveTopology topology);
}