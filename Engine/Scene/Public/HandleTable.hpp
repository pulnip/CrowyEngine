#pragma once

#include <vector>
#include "Assert.hpp"
#include "GenericHandle.hpp"
#include "Primitives.hpp"
#include "Semantics.hpp"
#include "StrongIndex.hpp"

namespace Crowy
{
    // SlotMap without the ownership
    template<typename T>
    class HandleTable{
    public:
        using Handle = GenericHandle<T>;

        struct SlotTag;
        struct IndexTag;
        // stable, survives element moves
        using Slot = StrongIndex<SlotTag>;
        // position in the caller's container, moves with the element
        using Index = StrongIndex<IndexTag>;

    private:
        struct Entry{
            Index index = Index::Invalid();
            usize generation = 0;

            bool IsValid() const noexcept{
                return index.IsValid();
            }

            void Reset() noexcept{
                index = Index::Invalid();
                ++generation;
            }
        };
        // indexed by slot number
        std::vector<Entry> entries;
        std::vector<Slot> freeSlots;

    public:
        HandleTable() = default;
        ~HandleTable() = default;
        CROWY_DECLARE_TRANSFERABLE(HandleTable)

        static Slot SlotOf(Handle handle) noexcept{
            return Slot{handle.GetIndex()};
        }

        Handle Acquire(Index index){
            CROWY_ASSERT(index.IsValid());

            // the slot only starts living once it carries an index
            auto slot = acquireSlot();
            entries[slot.value].index = index;

            return HandleOf(slot);
        }

        void Release(Handle handle) noexcept{
            CROWY_ASSERT(IsValid(handle));
            auto slot = SlotOf(handle);

            // expire old handles before the slot is reused
            entries[slot.value].Reset();
            freeSlots.push_back(slot);
        }

        // Answers for dead slots too, so it cannot lean on the guarded accessors
        bool IsValid(Handle handle) const noexcept{
            auto slot = SlotOf(handle);
            if(slot.value >= entries.size())
                return false;

            const auto& entry = entries[slot.value];

            return entry.IsValid() && entry.generation == handle.GetGeneration();
        }

        Index IndexOf(Handle handle) const noexcept{
            CROWY_ASSERT(IsValid(handle));

            return entries[SlotOf(handle).value].index;
        }
        Index IndexOf(Slot slot) const noexcept{
            CROWY_ASSERT(IsLiving(slot));

            return entries[slot.value].index;
        }
        usize GenerationOf(Slot slot) const noexcept{
            CROWY_ASSERT(IsLiving(slot));

            return entries[slot.value].generation;
        }

        // Points an already living slot at another position.
        void Bind(Slot slot, Index index) noexcept{
            CROWY_ASSERT(IsLiving(slot));
            CROWY_ASSERT(index.IsValid());

            entries[slot.value].index = index;
        }

        Handle HandleOf(Slot slot) const noexcept{
            CROWY_ASSERT(IsLiving(slot));

            return Handle{slot.value, GenerationOf(slot)};
        }

        bool IsLiving(Slot slot) const noexcept{
            return slot.value < entries.size() &&
                entries[slot.value].IsValid();
        }

        // Every slot ever handed out, living or not.
        usize SlotCount() const noexcept{
            return entries.size();
        }
        usize LiveCount() const noexcept{
            return entries.size() - freeSlots.size();
        }

    private:
        Slot acquireSlot(){
            if(!freeSlots.empty()){
                auto slot = freeSlots.back();
                freeSlots.pop_back();

                return slot;
            }

            // growing table for keeping slot vaild
            auto slot = Slot{entries.size()};
            entries.emplace_back();

            return slot;
        }
    };
}
