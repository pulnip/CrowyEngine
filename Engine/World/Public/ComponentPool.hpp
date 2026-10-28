#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "Actor.hpp"
#include "Primitives.hpp"
#include "Semantics.hpp"
#include "TypeOps.hpp"

namespace Crowy
{
    // one component type's rows, dense through its reflected lifetime
    // operations; a removal moves the last row into the hole
    class ComponentPool {
    private:
        struct FreeAligned {
            usize align = 0;

            void operator()(std::byte* rows) const noexcept;
        };

        using Rows = std::unique_ptr<std::byte[], FreeAligned>;
        using Owners = std::vector<ActorId>;
        using RowOfActor = std::vector<u32>;

        static constexpr u32 NoRow = ~0u;

        const TypeOps& ops;
        Rows rows;
        usize capacity = 0;
        Owners owners;
        // by actor slot
        RowOfActor rowOfActor;

    public:
        ~ComponentPool();
        CROWY_DECLARE_PINNED(ComponentPool)

        explicit ComponentPool(const TypeOps& ops);

        // moves `value` in as the actor's row
        void* Insert(ActorId actor, void* value);
        void Erase(ActorId actor);
        void Clear();

        // nullptr when the actor has no row here
        void* Find(ActorId actor) const noexcept;

        usize Count() const noexcept { return owners.size(); }
        ActorId OwnerAt(usize row) const noexcept { return owners[row]; }
        void* At(usize row) const noexcept { return rowAt(row); }

    private:
        void grow();
        std::byte* rowAt(usize row) const noexcept;
    };
}
