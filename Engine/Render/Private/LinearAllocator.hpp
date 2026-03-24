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
        const std::string name;

    public:
        LinearBufferAllocator(RHIDevice& device, const RHIBufferCreateDesc& desc, const std::string& name = "")
            : device(device)
            , desc(desc)
            , name(name){}
        ~LinearBufferAllocator() = default;

        void reset() noexcept{ nextIndex = 0; }

        // TODO. use offset later
        RHIBuffer& acquire(){
            if(nextIndex >= buffers.size()){
            #if defined(_DEBUG) || !defined(NDEBUG)
                buffers.push_back(device.createBuffer(desc, name));
            #else
                buffers.push_back(device.createBuffer(desc));
            #endif
            }

            return *buffers[nextIndex++].get();
        }
    };
}