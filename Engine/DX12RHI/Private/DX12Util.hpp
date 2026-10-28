#pragma once

#include <format>
#include <stdexcept>

#include <d3d12.h>
#include <dxgiformat.h>

#include "Primitives.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    template<typename T>
    inline void setDebugName(T& object, [[maybe_unused]] StrView name) {
#if CROWY_DEBUG_BUILD
        if(!name.empty()) {
            object.SetPrivateData(
                WKPDID_D3DDebugObjectName,
                static_cast<UINT>(name.size()),
                name.data()
            );
        }
#endif
    }

    DXGI_FORMAT convert(RHIPixelFormat);

    D3D12_COMPARISON_FUNC convert(RHIComparisonFunc);

    D3D12_BARRIER_SYNC convert(RHIBarrierSync);
    D3D12_BARRIER_ACCESS convert(RHIBarrierAccess);
    D3D12_BARRIER_LAYOUT convert(RHITextureLayout);

    Str HResultToString(HRESULT hr);
}

#define CHECK_HRESULT(expr, msg)                                               \
    do {                                                                       \
        if(const HRESULT hr = (expr); FAILED(hr)) [[unlikely]] {               \
            throw std::runtime_error(                                          \
                std::format("{}: {}", msg, ::Crowy::HResultToString(hr))       \
            );                                                                 \
        }                                                                      \
    } while(false)
