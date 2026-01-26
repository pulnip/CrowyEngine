#pragma once

#include <dxgiformat.h>
#include "RHIDefinitions.hpp"

namespace Crowy
{
    DXGI_FORMAT convertTextureFormat(RHITextureFormat);
    D3D11_COMPARISON_FUNC convertCompareFunc(RHIComparisonFunc);
}