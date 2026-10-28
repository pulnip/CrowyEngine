#pragma once

#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
    // record-time pairing of barrier halves, not GPU state tracking: each list
    // keeps its own, so parallel recording keeps working
    template<typename Payload>
    class RHIReleaseLedger {
    public:
        template<typename Barrier>
        struct Entry {
            Barrier barrier;
            Payload payload{};
            bool consumed = false;
        };

        using TextureEntry = Entry<RHITextureBarrier>;
        using BufferEntry = Entry<RHIBufferBarrier>;

    private:
        using TextureEntries =
            std::unordered_map<const RHITexture*, std::vector<TextureEntry>>;
        using BufferEntries = std::unordered_map<const RHIBuffer*, BufferEntry>;

        TextureEntries textures;
        BufferEntries buffers;

    public:
        // a release of the same range (a buffer has one) replaces the earlier
        // entry, which comes back when nothing consumed it
        std::optional<TextureEntry> Park(
            const RHITextureBarrier& release,
            Payload payload
        ) {
            auto& entries = textures[release.texture];
            for(auto& entry: entries) {
                if(entry.barrier.range == release.range)
                    return replace(entry, release, payload);
            }
            entries.push_back({.barrier = release, .payload = payload});

            return std::nullopt;
        }

        std::optional<BufferEntry> Park(
            const RHIBufferBarrier& release,
            Payload payload
        ) {
            const auto [found, inserted] = buffers.try_emplace(
                release.buffer,
                BufferEntry{.barrier = release, .payload = payload}
            );
            if(inserted)
                return std::nullopt;

            return replace(found->second, release, payload);
        }

        // full-edge equality doubles as the cross validation: both halves
        // carried the same value or they do not pair
        TextureEntry* Match(const RHITextureBarrier& acquire) {
            const auto found = textures.find(acquire.texture);
            if(found == textures.end())
                return nullptr;

            for(auto& entry: found->second) {
                if(entry.barrier == acquire)
                    return &entry;
            }

            return nullptr;
        }

        BufferEntry* Match(const RHIBufferBarrier& acquire) {
            const auto found = buffers.find(acquire.buffer);
            if(found == buffers.end() || found->second.barrier != acquire)
                return nullptr;

            return &found->second;
        }

        template<typename F>
        void ForEachUnconsumed(F&& visit) const {
            for(const auto& [_, entries]: textures) {
                for(const auto& entry: entries) {
                    if(!entry.consumed)
                        visit(entry);
                }
            }
            for(const auto& [_, entry]: buffers) {
                if(!entry.consumed)
                    visit(entry);
            }
        }

        void Clear() {
            textures.clear();
            buffers.clear();
        }

    private:
        template<typename Barrier>
        static std::optional<Entry<Barrier>> replace(
            Entry<Barrier>& entry,
            const Barrier& release,
            Payload payload
        ) {
            auto displaced = std::exchange(
                entry,
                Entry<Barrier>{.barrier = release, .payload = payload}
            );
            if(displaced.consumed)
                return std::nullopt;

            return displaced;
        }
    };
}
