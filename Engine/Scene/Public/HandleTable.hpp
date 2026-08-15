#pragma once

#include <vector>
#include "Assert.hpp"
#include "GenericHandle.hpp"
#include "Primitives.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    // SlotMap without the ownership
    template<typename T>
    class HandleTable{
    public:
        using Handle = GenericHandle<T>;

    private:
        // indexed by slot number
        std::vector<usize> indexes;
        std::vector<usize> generations;
        std::vector<usize> freeSlots;

    public:
        HandleTable() = default;
        ~HandleTable() = default;
        CROWY_DECLARE_TRANSFERABLE(HandleTable)

        Handle Acquire(usize index){
            CROWY_ASSERT(index != Handle::INVALID_INDEX);

            auto slot = acquireSlot();
            indexes[slot] = index;

            return Handle(slot, generations[slot]);
        }

        void Release(Handle handle) noexcept{
            CROWY_ASSERT(IsValid(handle));
            auto slot = handle.GetIndex();

            // expire old handles before the slot is reused
            ++generations[slot];
            indexes[slot] = Handle::INVALID_INDEX;
            freeSlots.push_back(slot);
        }

        bool IsValid(Handle handle) const noexcept{
            auto slot = handle.GetIndex();
            if(slot >= indexes.size())
                return false;
            if(generations[slot] != handle.GetGeneration())
                return false;

            return indexes[slot] != Handle::INVALID_INDEX;
        }

        usize IndexOf(Handle handle) const noexcept{
            CROWY_ASSERT(IsValid(handle));

            return indexes[handle.GetIndex()];
        }
        usize IndexOfSlot(usize slot) const noexcept{
            CROWY_ASSERT(isLiving(slot));

            return indexes[slot];
        }

        // Points an already living slot at another position.
        void Bind(usize slot, usize index) noexcept{
            CROWY_ASSERT(isLiving(slot));
            CROWY_ASSERT(index != Handle::INVALID_INDEX);

            indexes[slot] = index;
        }

        Handle HandleOfSlot(usize slot) const noexcept{
            CROWY_ASSERT(isLiving(slot));

            return Handle(slot, generations[slot]);
        }

        // Every slot ever handed out, living or not.
        usize SlotCount() const noexcept{
            return indexes.size();
        }
        usize LiveCount() const noexcept{
            return indexes.size() - freeSlots.size();
        }

    private:
        usize acquireSlot(){
            if(!freeSlots.empty()){
                auto slot = freeSlots.back();
                freeSlots.pop_back();

                return slot;
            }

            // growing table for keeping slot vaild
            auto slot = indexes.size();
            indexes.push_back(Handle::INVALID_INDEX);
            generations.push_back(0);

            return slot;
        }

        bool isLiving(usize slot) const noexcept{
            return slot < indexes.size() &&
                indexes[slot] != Handle::INVALID_INDEX;
        }
    };
}
