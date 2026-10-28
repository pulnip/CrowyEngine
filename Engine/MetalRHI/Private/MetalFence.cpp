#include "MetalFence.hpp"

#include <chrono>
#include <thread>

#include <Metal/Metal.hpp>
#include <dispatch/dispatch.h>

namespace Crowy
{
    MetalFence::MetalFence(MTL::Device& device, u64 initialValue) {
        sharedEvent = NS::TransferPtr(device.newSharedEvent());
        sharedEvent->setSignaledValue(initialValue);
    }

    MetalFence::~MetalFence() = default;

    void MetalFence::WaitCPU(u64 waitValue, u64 timeoutMs) {
        using namespace std::chrono;

        auto startTime = high_resolution_clock::now();

        while(sharedEvent->signaledValue() < waitValue) {
            if(timeoutMs > 0) {
                auto elapsed = duration_cast<milliseconds>(
                                   high_resolution_clock::now() - startTime
                )
                                   .count();

                if(elapsed >= timeoutMs)
                    return;
            }

            std::this_thread::yield();
        }
    }

    u64 MetalFence::GetValue() const {
        return sharedEvent->signaledValue();
    }

    bool MetalFence::IsComplete(u64 value) const {
        return sharedEvent->signaledValue() >= value;
    }

    void MetalFence::Encode(MTL::CommandBuffer& cmdBuffer, u64 value) {
        cmdBuffer.encodeSignalEvent(sharedEvent.get(), value);
    }
}
