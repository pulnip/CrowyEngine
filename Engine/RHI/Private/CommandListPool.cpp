#include "Assert.hpp"
#include "CommandListPool.hpp"
#include "RHICommandList.hpp"
#include "RHIDefinitions.hpp"
#include "RHIDevice.hpp"
#include "RHIFence.hpp"
#include "RHISwapchain.hpp"

namespace Crowy
{
    CommandListPool::CommandListPool(RHIDevice& device)
        : device(device){}

    CommandListPool::~CommandListPool() = default;

    void CommandListPool::BeginFrame(){
        frameIndex = (frameIndex + 1) % RHI_FRAMES_IN_FLIGHT;
        auto& slot = slots[frameIndex];

        slot.cmdLists.clear();
        slot.nextIndex = 0;
    }

    RHICommandList& CommandListPool::Acquire(){
        SMOL_ASSERT(frameIndex < RHI_FRAMES_IN_FLIGHT);
        auto& slot = slots[frameIndex];

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

    void CommandListPool::SubmitFrame(
        RHIFence& fence,
        u64 fenceValue
    ){
        SMOL_ASSERT(frameIndex < RHI_FRAMES_IN_FLIGHT);
        auto& slot = slots[frameIndex];

        if(slot.nextIndex == 0) [[unlikely]] {
            // empty cmdList submit pattern
            // for if none of renderer use cmdList in this frame
            auto& cmdList = Acquire();
            cmdList.Begin();
            cmdList.Close();
        }

        device.SignalFence(*slot.cmdLists.back(), fence, fenceValue);

        for(auto& cmdList: slot.cmdLists){
            device.Submit(*cmdList);
        }
    }
}
