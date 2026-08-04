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
    DXGI_FORMAT convert(RHIPixelFormat);
    RHIPixelFormat convert(DXGI_FORMAT);

    D3D12_COMPARISON_FUNC convert(RHIComparisonFunc);

    D3D12_BARRIER_SYNC convert(RHIBarrierSync);
    D3D12_BARRIER_ACCESS convert(RHIBarrierAccess);
    D3D12_BARRIER_LAYOUT convert(RHITextureLayout);

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
