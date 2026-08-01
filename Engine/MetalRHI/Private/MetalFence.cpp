#include <chrono>
#include <thread>
#include <dispatch/dispatch.h>
#include <Metal/Metal.hpp>
#include "MetalFence.hpp"

namespace Crowy
{
    MetalFence::MetalFence(
        MTL::Device& device,
        u64 initialValue
    ){
        sharedEvent = NS::TransferPtr(device.newSharedEvent());
        sharedEvent->setSignaledValue(initialValue);
    }

    MetalFence::~MetalFence() = default;

    void MetalFence::WaitCPU(u64 waitValue, u64 timeoutMs){
        using namespace std::chrono;

        auto startTime = high_resolution_clock::now();

        while(sharedEvent->signaledValue() < waitValue){
            if(timeoutMs > 0){
                auto elapsed = duration_cast<milliseconds>(
                    high_resolution_clock::now() - startTime
                ).count();

                if (elapsed >= timeoutMs)
                    return;
            }

            std::this_thread::yield();
        }

        // auto sem = dispatch_semaphore_create(0);

        // sharedEvent->notifyListener(listener, waitValue,
        //     ^(MTL::SharedEvent*, u64){
        //         dispatch_semaphore_signal(sem);
        //     }
        // );

        // if(timeoutMs == 0){
        //     dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
        // }
        // else{
        //     dispatch_time_t timeout = dispatch_time(
        //         DISPATCH_TIME_NOW,
        //         timeoutMs * NSEC_PER_MSEC
        //     );
        //     dispatch_semaphore_wait(sem, timeout);
        // }

        // dispatch_release(sem);
    }

    u64 MetalFence::GetValue(){
        return sharedEvent->signaledValue();
    }

    bool MetalFence::IsComplete(u64 value){
        return sharedEvent->signaledValue() >= value;
    }

    void MetalFence::Encode(MTL::CommandBuffer& cmdBuffer, u64 value){
        cmdBuffer.encodeSignalEvent(sharedEvent.get(), value);
    }
}
