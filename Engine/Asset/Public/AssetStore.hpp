#pragma once

#include <algorithm>
#include <expected>
#include <format>
#include <utility>
#include <vector>

#include "Assert.hpp"
#include "AssetRef.hpp"
#include "Function.hpp"
#include "PackedTable.hpp"
#include "Primitives.hpp"
#include "Semantics.hpp"
#include "StringUtil.hpp"

namespace Crowy
{
    template<typename T>
    class AssetHandle;

    // Loads each asset once and keeps it while a handle holds it; a failed
    // load is the caller's to report and leaves nothing behind. The last
    // handle only marks it, and Collect releases what is still unheld. One
    // thread owns a store, so the count is not atomic.
    template<typename T>
    class AssetStore {
    public:
        using Loaded = std::expected<T, Str>;
        using Loader = std::move_only_function<Loaded(const AssetRef&)>;
        using Releaser = std::move_only_function<void(T)>;
        using Handle = AssetHandle<T>;
        using LoadResult = std::expected<Handle, Str>;

    private:
        friend class AssetHandle<T>;

        struct Entry {
            AssetRef ref;
            T value;
            u32 holders = 0;
        };
        using Entries = PackedTable<Entry>;
        using EntryHandle = typename Entries::Handle;
        using EntryHandles = std::vector<EntryHandle>;

        Loader load;
        Releaser release;
        Entries entries;
        // weak: an entry leaves the map when it is released
        StringHashMap<EntryHandle> entryOfPath;
        // may repeat an entry that was held again and dropped again
        EntryHandles unheld;

    public:
        ~AssetStore() {
            [[maybe_unused]] const bool unheldAll =
                std::ranges::all_of(entries.All(), [](const Entry& entry) {
                    return entry.holders == 0;
                });
            CROWY_ASSERT(unheldAll, "an asset handle outlives its store");

            Collect();
        }
        CROWY_DECLARE_PINNED(AssetStore)

        // without a releaser a release only destroys the value
        explicit AssetStore(Loader load, Releaser release = {})
            : load(std::move(load)), release(std::move(release)) {}

        // the error names the path, so a list of them reads on its own
        [[nodiscard]] LoadResult Load(const AssetRef& ref) {
            CROWY_ASSERT(!ref.IsNone(), "loading a ref that names no asset");

            auto path = ref.ToString();
            // an entry marked but not yet collected is held again as it is
            if(const auto it = entryOfPath.find(path); it != entryOfPath.end())
                return Handle(*this, it->second);

            auto loaded = load(ref);
            if(!loaded)
                return std::unexpected(
                    std::format("{}: {}", path, loaded.error())
                );

            const auto entry = entries.Add(
                Entry{
                    .ref = ref,
                    .value = std::move(*loaded),
                }
            );
            entryOfPath.emplace(std::move(path), entry);

            return Handle(*this, entry);
        }

        // Releases every asset no handle holds, including any a release
        // leaves unheld in turn.
        void Collect() {
            while(!unheld.empty()) {
                const auto candidates = std::exchange(unheld, EntryHandles{});
                for(const auto entry: candidates) {
                    if(!entries.IsValid(entry))
                        continue;

                    auto& row = entries.GetRef(entry);
                    if(row.holders != 0)
                        continue;

                    entryOfPath.erase(row.ref.ToString());
                    auto value = std::move(row.value);
                    entries.Remove(entry);

                    if(release)
                        release(std::move(value));
                }
            }
        }

        // the assets loaded and not yet released
        usize Count() const noexcept { return entries.Count(); }

    private:
        void hold(EntryHandle entry) { ++entries.GetRef(entry).holders; }

        void drop(EntryHandle entry) {
            auto& row = entries.GetRef(entry);
            CROWY_ASSERT(row.holders > 0, "an asset dropped more than held");

            if(--row.holders == 0)
                unheld.push_back(entry);
        }

        const Entry& entryOf(EntryHandle entry) const {
            return entries.GetRef(entry);
        }
    };

    // Holds one asset of a store. A reference from it lasts until the
    // store's next Collect, as rows move when one is released.
    template<typename T>
    class AssetHandle {
    private:
        friend class AssetStore<T>;

        using Store = AssetStore<T>;
        using EntryHandle = typename Store::EntryHandle;

        Store* store = nullptr;
        EntryHandle entry = EntryHandle::InvalidHandle();

    public:
        AssetHandle() = default;
        ~AssetHandle() { reset(); }

        AssetHandle(const AssetHandle& other)
            : store(other.store), entry(other.entry) {
            if(store != nullptr)
                store->hold(entry);
        }

        AssetHandle(AssetHandle&& other) noexcept
            : store(std::exchange(other.store, nullptr)),
              entry(std::exchange(other.entry, EntryHandle::InvalidHandle())) {}

        // one assignment for copy and move, safe against itself
        AssetHandle& operator=(AssetHandle other) noexcept {
            std::swap(store, other.store);
            std::swap(entry, other.entry);
            return *this;
        }

        explicit operator bool() const noexcept { return store != nullptr; }

        const T& operator*() const { return get().value; }
        const T* operator->() const { return &get().value; }
        const AssetRef& Ref() const { return get().ref; }

    private:
        AssetHandle(Store& store, EntryHandle entry)
            : store(&store), entry(entry) {
            this->store->hold(this->entry);
        }

        const typename Store::Entry& get() const {
            CROWY_ASSERT(store != nullptr, "an empty asset handle");

            return store->entryOf(entry);
        }

        void reset() noexcept {
            if(store != nullptr)
                std::exchange(store, nullptr)->drop(entry);
        }
    };
}
