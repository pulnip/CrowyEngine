#pragma once

#include <vector>
#include "RHIBuffer.hpp"
#include "RHIDefinitions.hpp"
#include "RHIDevice.hpp"

namespace Crowy
{
    class LinearBufferAllocator{
    private:
        RHIDevice& device;
        std::vector<RHIBufferPtr> buffers;
        size_t nextIndex = 0;
        const RHIBufferCreateDesc desc;

    public:
        LinearBufferAllocator(RHIDevice& device, const RHIBufferCreateDesc& desc)
            : device(device), desc(desc){}
        ~LinearBufferAllocator() = default;

        void reset() noexcept{ nextIndex = 0; }

        // TODO. use offset later
        RHIBuffer& acquire(){
            if(nextIndex >= buffers.size())
                buffers.push_back(device.createBuffer(desc));

            return *buffers[nextIndex++].get();
        }
    };
}