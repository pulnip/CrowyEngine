#include "Assert.hpp"
#include "CommandListPool.hpp"
#include "RHICommandList.hpp"
#include "RHIDevice.hpp"

namespace Crowy
{
    CommandListPool::CommandListPool(RHIDevice& device)
        : device(device){}

    CommandListPool::~CommandListPool() = default;

    void CommandListPool::BeginFrame(u64 frame){
        CROWY_ASSERT(frame == this->frame + 1,
            "the pool records frame {} after frame {}", frame, this->frame
        );
        this->frame = frame;

        // the lists stay: the pacer guarantees frame N - RHI_FRAMES_IN_FLIGHT,
        // their last user, has completed, so Begin() may reset them
        auto& slot = slots[currentIndex()];
        slot.nextIndex = 0;

    #if CROWY_FRAME_STATS
        frameStats = RHIFrameStats{};
    #endif
    }

    RHICommandList& CommandListPool::Acquire(){
        auto& slot = slots[currentIndex()];

        if(slot.nextIndex >= slot.cmdLists.size()){
            // reserve new commandList, if not enough
            auto newCmdList = device.CreateCommandList();
            slot.cmdLists.emplace_back(std::move(newCmdList));

        #if CROWY_FRAME_STATS
            ++frameStats.commandListCreateCount;
        #endif
        }

        auto acquireIndex = slot.nextIndex++;
        auto acquiredCmdList = slot.cmdLists[acquireIndex].get();
        CROWY_ASSERT(acquiredCmdList != nullptr);
        return *acquiredCmdList;
    }

    std::vector<RHICommandList*> CommandListPool::ExtractRecorded(){
        auto& slot = slots[currentIndex()];

        if(slot.nextIndex == 0) [[unlikely]] {
            // empty cmdList submit pattern
            // for if none of renderer use cmdList in this frame
            auto& cmdList = Acquire();
            cmdList.Begin();
            cmdList.Close();
        }

        // cmdLists is a high-water mark now, so only what was handed out
        // this frame gets submitted
        std::vector<RHICommandList*> cmdLists(slot.nextIndex);
        for(usize i=0; i<cmdLists.size(); ++i){
            cmdLists[i] = slot.cmdLists[i].get();

        #if CROWY_FRAME_STATS
            frameStats += cmdLists[i]->GetStats();
        #endif
        }

        return cmdLists;
    }
}
