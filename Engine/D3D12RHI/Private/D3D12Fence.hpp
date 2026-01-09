#pragma once

#include <chrono>
#include <thread>
#include <d3d12.h>
#include <wrl/client.h>
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHIFence.hpp"
#endif

using Microsoft::WRL::ComPtr;

namespace Crowy
{
    class D3D12Fence
#ifndef USE_STATIC_RHI
        : public RHIFence
#endif
    {
    private:
        ComPtr<ID3D12Fence> fence;
        HANDLE fenceEvent = nullptr;

    public:
        D3D12Fence(
            ID3D12Device* device,
            uint64_t initialValue
        ){
            if(FAILED(device->CreateFence(initialValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)))){
                throw std::runtime_error("Failed to create fence");
            }

            fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            if(!fenceEvent){
                throw std::runtime_error("Failed to create fence event");
            }
        }

        ~D3D12Fence(){
            if(fenceEvent){
                CloseHandle(fenceEvent);
            }
        }

        DECLARE_NON_COPYABLE(D3D12Fence)

        void waitCPU(uint64_t waitValue, uint64_t timeoutMs) RHI_OVERRIDE{
            if(fence->GetCompletedValue() < waitValue){
                fence->SetEventOnCompletion(waitValue, fenceEvent);

                DWORD timeout = (timeoutMs == 0) ? INFINITE : static_cast<DWORD>(timeoutMs);
                WaitForSingleObject(fenceEvent, timeout);
            }
        }

        uint64_t getValue() RHI_OVERRIDE{
            return fence->GetCompletedValue();
        }

        bool isComplete(uint64_t value) RHI_OVERRIDE{
            return fence->GetCompletedValue() >= value;
        }

        ID3D12Fence* get() const{
            return fence.Get();
        }
    };
}