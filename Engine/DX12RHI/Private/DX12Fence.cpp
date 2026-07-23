#include "DX12Fence.hpp"
#include "DX12Util.hpp"

namespace Crowy
{
    DX12Fence::DX12Fence(
        Device& device,
        u64 initialValue
    ){
        CHECK_HRESULT(device.CreateFence(
            initialValue,
            D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(&fence)
        ), "Failed to create fence");

        fenceEvent = CreateEvent(
            nullptr,
            FALSE,
            FALSE,
            nullptr
        );
        if(fenceEvent == nullptr){
            throw std::runtime_error("Failed to create fence event");
        }
    }

    DX12Fence::~DX12Fence(){
        if(fenceEvent != nullptr){
            CloseHandle(fenceEvent);
            fenceEvent = nullptr;
        }
    }

    void DX12Fence::WaitCPU(u64 waitValue, u64 timeoutMs){
        if(fence->GetCompletedValue() < waitValue){
            CHECK_HRESULT(fence->SetEventOnCompletion(
                waitValue, fenceEvent
            ), "Failed to register fence completion event");

            DWORD timeout = (timeoutMs == 0) ? INFINITE : static_cast<DWORD>(timeoutMs);
            WaitForSingleObject(fenceEvent, timeout);
        }
    }

    u64 DX12Fence::GetValue(){
        return fence->GetCompletedValue();
    }

    bool DX12Fence::IsComplete(u64 value){
        return fence->GetCompletedValue() >= value;
    }
}
