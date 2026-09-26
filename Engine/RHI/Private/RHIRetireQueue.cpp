#include "RHIRetireQueue.hpp"

#include <utility>

namespace Crowy
{
    void RHIRetireQueue::Defer(Reclaim reclaim){
        pending.push_back(std::move(reclaim));
    }

    void RHIRetireQueue::Tag(u64 value){
        for(auto& reclaim: pending){
            tagged.push_back(Entry{
                .tag = value,
                .reclaim = std::move(reclaim)
            });
        }
        pending.clear();
    }

    void RHIRetireQueue::Collect(u64 completed){
        // Tag() only ever appends with a strictly increasing value, so
        // `tagged` is already sorted ascending - popping the front until
        // it outruns `completed` is enough
        while(!tagged.empty() && tagged.front().tag <= completed){
            tagged.front().reclaim();
            tagged.pop_front();
        }
    }

    void RHIRetireQueue::CollectAll(){
        // destroying a reclaim may Defer another (D3D12 frees a descriptor
        // that way), so each round takes the queues out until none arrives
        while(!tagged.empty() || !pending.empty()){
            auto entries = std::exchange(tagged, {});
            for(auto& entry: entries){
                entry.reclaim();
            }
            entries.clear();

            auto reclaims = std::exchange(pending, {});
            for(auto& reclaim: reclaims){
                reclaim();
            }
        }
    }
}
