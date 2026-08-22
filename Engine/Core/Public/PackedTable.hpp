#pragma once

#include <span>
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
            const auto index = Index{rows.size()};

            rows.push_back(row);
            const auto handle = slots.Acquire(index);
            slotOfRow.push_back(Slots::SlotOf(handle));

            return handle;
        }

        // the row's current position, valid until the next Remove
        usize IndexOf(Handle handle) const noexcept {
            CROWY_ASSERT(IsValid(handle));

            return slots.IndexOf(handle).value;
        }

        const T& Read(Handle handle) const noexcept {
            CROWY_ASSERT(IsValid(handle));

            return rows[IndexOf(handle)];
        }

        void Write(Handle handle, const T& row) {
            CROWY_ASSERT(IsValid(handle));

            rows[IndexOf(handle)] = row;
        }

        void Remove(Handle handle) {
            CROWY_ASSERT(IsValid(handle));

            const auto index = IndexOf(handle);
            const auto last = rows.size() - 1;

            if(index != last) {
                rows[index] = rows[last];
                slotOfRow[index] = slotOfRow[last];
                // the moved row's handle has to follow it
                slots.Bind(slotOfRow[index], Index{index});
            }

            rows.pop_back();
            slotOfRow.pop_back();
            slots.Release(handle);
        }

        // expires every handle ever issued
        void Clear() noexcept {
            rows.clear();
            slotOfRow.clear();
            slots = Slots{};
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
    };
}
