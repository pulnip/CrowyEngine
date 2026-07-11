#pragma once

#include <format>
#include <stdexcept>
#include <dxgiformat.h>
#include <d3d12.h>
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"

#define BYTES(x) &(x), sizeof(x)

namespace Crowy
{
    DXGI_FORMAT convert(RHIPixelFormat,
        bool isShaderResource=true,
        bool isDepthTarget=false
    );
    D3D12_COMPARISON_FUNC convert(RHIComparisonFunc);
    D3D12_RESOURCE_STATES convert(RHIResourceState);

    RHIPixelFormat convert(DXGI_FORMAT);

    Str HResultToString(HRESULT hr);
}

#define CHECK_HRESULT(expr, msg) \
    do{ \
        if(const HRESULT hr = (expr); FAILED(hr)) [[unlikely]]{ \
            throw std::runtime_error(std::format( \
                "{}: {}", msg, ::Crowy::HResultToString(hr) \
            )); \
        } \
    } while(false)
