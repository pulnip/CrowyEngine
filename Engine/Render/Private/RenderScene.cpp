#include "RenderScene.hpp"

namespace Crowy
{
    PrimitiveHandle RenderScene::Add(const PrimitiveSnapshot& snapshot) {
        const auto index = PrimitiveTable::Index{primitives.size()};

        primitives.push_back(snapshot);
        const auto handle = slots.Acquire(index);
        slotOfRow.push_back(PrimitiveTable::SlotOf(handle));

        return handle;
    }

    void RenderScene::Write(
        PrimitiveHandle handle,
        const PrimitiveSnapshot& snapshot
    ) {
        CROWY_ASSERT(IsValid(handle));

        primitives[slots.IndexOf(handle).value] = snapshot;
    }

    void RenderScene::Remove(PrimitiveHandle handle) {
        CROWY_ASSERT(IsValid(handle));

        const auto index = slots.IndexOf(handle).value;
        const auto last = primitives.size() - 1;

        if(index != last) {
            primitives[index] = primitives[last];
            slotOfRow[index] = slotOfRow[last];
            // the moved row's handle has to follow it
            slots.Bind(slotOfRow[index], PrimitiveTable::Index{index});
        }

        primitives.pop_back();
        slotOfRow.pop_back();
        slots.Release(handle);
    }

    void RenderScene::Clear() noexcept {
        primitives.clear();
        slotOfRow.clear();
        slots = PrimitiveTable{};
    }
}
