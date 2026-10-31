#pragma once

#include <optional>
#include <utility>
#include <vector>

#include "Assert.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    // A value beside each GenericHandle, stored by the handle's slot. A handle
    // its slot no longer holds finds nothing and erases nothing.
    template<typename Handle, typename V>
    class HandleMap {
    private:
        using Entry = std::pair<Handle, V>;

        std::vector<Entry> entries;

    public:
        void Set(Handle handle, V value) {
            CROWY_ASSERT(handle.IsValid());
            const auto slot = handle.GetIndex();
            if(slot >= entries.size())
                entries.resize(slot + 1);
            entries[slot] = Entry{handle, std::move(value)};
        }

        void Erase(Handle handle) {
            const auto slot = handle.GetIndex();
            if(slot < entries.size() && entries[slot].first == handle)
                entries[slot] = Entry{};
        }

        std::optional<V> Find(Handle handle) const {
            const auto slot = handle.GetIndex();
            if(slot >= entries.size() || entries[slot].first != handle)
                return std::nullopt;

            return entries[slot].second;
        }

        void Clear() noexcept { entries.clear(); }
    };
}
