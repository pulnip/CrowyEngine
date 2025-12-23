#pragma once

namespace Crowy
{
    class RHIBuffer{
    public:
        RHIBuffer() = default;
        virtual ~RHIBuffer() = default;
        RHIBuffer(const RHIBuffer&) = delete;
        RHIBuffer(RHIBuffer&&) = default;
        RHIBuffer& operator=(const RHIBuffer&) = delete;
        RHIBuffer& operator=(RHIBuffer&&) = default;
    };
}
