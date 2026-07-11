#include "Assert.hpp"
#include "DX12CommandList.hpp"
#include "DX12CommandListPool.hpp"
#include "DX12Device.hpp"

namespace Crowy
{
    DX12CommandListPool::DX12CommandListPool(DX12Device& device)
        : device(device)
        , frameIndex(device.GetFrameIndexRef())
    {}

    DX12CommandListPool::~DX12CommandListPool() = default;

    void DX12CommandListPool::BeginFrame(){
        auto& slot = slots[currentIndex()];

        slot.cmdLists.clear();
        slot.nextIndex = 0;
    }

    DX12CommandList& DX12CommandListPool::Acquire(){
        auto& slot = slots[currentIndex()];

        if(slot.nextIndex >= slot.cmdLists.size()){
            // reserve new commandList, if not enough
            auto newCmdList = device.CreateCommandList();
            slot.cmdLists.emplace_back(std::move(newCmdList));
        }

        auto acquireIndex = slot.nextIndex++;
        auto acquiredCmdList = slot.cmdLists[acquireIndex].get();
        SMOL_ASSERT(acquiredCmdList != nullptr);
        return *acquiredCmdList;
    }

    void DX12CommandListPool::SubmitFrame(){
        auto& slot = slots[currentIndex()];

        if(slot.nextIndex == 0) [[unlikely]] {
            // empty cmdList submit pattern
            // for if none of renderer use cmdList in this frame
            auto& cmdList = Acquire();
            cmdList.Begin();
            cmdList.Close();
        }

        std::vector<DX12CommandList*> cmdLists(slot.cmdLists.size());
        for(usize i=0; i<cmdLists.size(); ++i){
            cmdLists[i] = slot.cmdLists[i].get();
        }
        device.Submit(cmdLists);
    }
}
