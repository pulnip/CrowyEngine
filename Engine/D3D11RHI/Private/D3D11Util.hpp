#pragma once

#include <dxgiformat.h>
#include "RHIDefinitions.hpp"

namespace Crowy
{
    DXGI_FORMAT convertTextureFormat(RHIPixelFormat, bool isShaderResource=true, bool isDepthTarget=false);
    D3D11_COMPARISON_FUNC convertCompareFunc(RHIComparisonFunc);
}