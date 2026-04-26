#pragma once

#ifndef USE_STATIC_RHI
    #include "RHIFrameScope.hpp"
#endif

namespace Crowy
{
    class D3D11FrameScope
#ifndef USE_STATIC_RHI
        : public RHIFrameScope
#endif
    {
    public:
        D3D11FrameScope() = default;
        ~D3D11FrameScope() = default;
    };
}