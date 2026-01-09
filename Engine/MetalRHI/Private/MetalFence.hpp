#pragma once

#include <chrono>
#include <thread>
#include <Metal/Metal.hpp>
#include <dispatch/dispatch.h>
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHIFence.hpp"
#endif

namespace Crowy
{
    class MetalFence
#ifndef USE_STATIC_RHI
        : public RHIFence
#endif
    {
    private:
        MTL::SharedEvent* sharedEvent = nullptr;

    public:
        MetalFence(
            MTL::Device* device,
            uint64_t initialValue
        ){
            sharedEvent = device->newSharedEvent();
            sharedEvent->setSignaledValue(initialValue);
        }

        ~MetalFence(){
            if(sharedEvent) sharedEvent->release();
        }

        CROWY_DECLARE_NON_COPYABLE(MetalFence)

        void waitCPU(uint64_t waitValue, uint64_t timeoutMs) RHI_OVERRIDE{
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
            //     ^(MTL::SharedEvent*, uint64_t){
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

        uint64_t getValue() RHI_OVERRIDE{
            return sharedEvent->signaledValue();
        }

        bool isComplete(uint64_t value) RHI_OVERRIDE{
            return sharedEvent->signaledValue() >= value;
        }

        MTL::SharedEvent* getSharedEvent() const{
            return sharedEvent;
        }
    };
}