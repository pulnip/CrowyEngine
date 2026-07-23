#include "Assert.hpp"
#include "CommandListPool.hpp"
#include "RHICommandList.hpp"
#include "RHIDevice.hpp"

namespace Crowy
{
    CommandListPool::CommandListPool(RHIDevice& device)
        : device(device)
        , frameIndex(device.GetFrameIndexRef())
    {}

    CommandListPool::~CommandListPool() = default;

    void CommandListPool::BeginFrame(){
        auto& slot = slots[currentIndex()];

        slot.cmdLists.clear();
        slot.nextIndex = 0;
    }

    RHICommandList& CommandListPool::Acquire(){
        auto& slot = slots[currentIndex()];

        if(slot.nextIndex >= slot.cmdLists.size()){
            // reserve new commandList, if not enough
            auto newCmdList = device.CreateCommandList();
            slot.cmdLists.emplace_back(std::move(newCmdList));
        }

        auto acquireIndex = slot.nextIndex++;
        auto acquiredCmdList = slot.cmdLists[acquireIndex].get();
        CROWY_ASSERT(acquiredCmdList != nullptr);
        return *acquiredCmdList;
    }

    void CommandListPool::SubmitFrame(){
        auto& slot = slots[currentIndex()];

        if(slot.nextIndex == 0) [[unlikely]] {
            // empty cmdList submit pattern
            // for if none of renderer use cmdList in this frame
            auto& cmdList = Acquire();
            cmdList.Begin();
            cmdList.Close();
        }

        std::vector<RHICommandList*> cmdLists(slot.cmdLists.size());
        for(usize i=0; i<cmdLists.size(); ++i){
            cmdLists[i] = slot.cmdLists[i].get();
        }
        device.Submit(cmdLists);
    }
}
