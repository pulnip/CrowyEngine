#pragma once

#include <algorithm>
#include <span>
#include <utility>
#include <vector>

#include "Assert.hpp"
#include "GenericHandle.hpp"
#include "HandleTable.hpp"
#include "Primitives.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    // A packed array that removes by swapping the last row into the hole.
    // Handles survive that move; references and positions do not.
    template<typename T>
    class PackedTable {
    public:
        using Handle = GenericHandle<T>;
        using Slots = HandleTable<T>;
        using Slot = typename Slots::Slot;
        using Index = typename Slots::Index;
        using Rows = std::vector<T>;
        using RowOwners = std::vector<Slot>;

    private:
        Slots slots;
        Rows rows;
        // the direction HandleTable does not store, needed by swap-remove
        RowOwners slotOfRow;

    public:
        PackedTable() = default;
        ~PackedTable() = default;
        CROWY_DECLARE_TRANSFERABLE(PackedTable)

        Handle Add(const T& row) {
            rows.push_back(row);

            return bindLastRow();
        }

        // for a row that cannot be copied, such as one owning a GPU resource
        Handle Add(T&& row) {
            rows.push_back(std::move(row));

            return bindLastRow();
        }

        // the row's current position, valid until the next Remove
        usize IndexOf(Handle handle) const noexcept {
            CROWY_ASSERT(IsValid(handle));

            return slots.IndexOf(handle).value;
        }

        // the handle of the row at this position, valid until the next Remove
        Handle HandleAt(usize index) const noexcept {
            CROWY_ASSERT(index < rows.size());

            return slots.HandleOf(slotOfRow[index]);
        }

        auto& GetRef(this auto& self, Handle handle) noexcept {
            CROWY_ASSERT(self.IsValid(handle));

            return self.rows[self.IndexOf(handle)];
        }

        void Remove(Handle handle) {
            CROWY_ASSERT(IsValid(handle));

            const auto index = IndexOf(handle);
            const auto last = rows.size() - 1;

            if(index != last) {
                rows[index] = std::move(rows[last]);
                slotOfRow[index] = slotOfRow[last];
                // the moved row's handle has to follow it
                slots.Bind(slotOfRow[index], Index{index});
            }

            rows.pop_back();
            slotOfRow.pop_back();
            slots.Release(handle);
        }

        // Expires every handle ever issued. Each living slot is released,
        // so its generation moves on and a handle from before stays dead
        // once the slot is reused; lowest slots are reused first, so a
        // table rebuilt in the same order gets the same slots back.
        void Clear() {
            std::ranges::sort(slotOfRow, std::greater{}, &Slot::value);
            for(const auto slot: slotOfRow)
                slots.Release(slots.HandleOf(slot));
            rows.clear();
            slotOfRow.clear();
        }

        bool IsValid(Handle handle) const noexcept {
            return slots.IsValid(handle);
        }

        usize Count() const noexcept { return rows.size(); }
        bool IsEmpty() const noexcept { return rows.empty(); }

        const T& At(usize index) const noexcept {
            CROWY_ASSERT(index < rows.size());

            return rows[index];
        }

        std::span<const T> All() const noexcept { return rows; }

    private:
        Handle bindLastRow() {
            const auto handle = slots.Acquire(Index{rows.size() - 1});
            slotOfRow.push_back(Slots::SlotOf(handle));

            return handle;
        }
    };
}
