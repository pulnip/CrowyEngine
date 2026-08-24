#include "RHIRetireQueue.hpp"

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
        for(auto& entry: tagged){
            entry.reclaim();
        }
        tagged.clear();

        for(auto& reclaim: pending){
            reclaim();
        }
        pending.clear();
    }
}
