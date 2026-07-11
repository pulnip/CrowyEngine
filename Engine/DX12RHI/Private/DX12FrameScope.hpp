#pragma once

#include "RHIFrameScope.hpp"

namespace Crowy
{
    class DX12FrameScope final: public RHIFrameScope{
    public:
        DX12FrameScope() = default;
        ~DX12FrameScope() = default;
    };
}
