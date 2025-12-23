#pragma once

#include <atomic>
#include <cstdint>
#include <string>

namespace Crowy
{
    // Base class for all RHI resources
    class RHIResource{
    protected:
        std::atomic<uint32_t> refCount;

    public:
        RHIResource()
            :refCount(1){}

        virtual ~RHIResource() = default;
    };
}