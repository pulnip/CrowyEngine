#include "ComponentPool.hpp"

#include <algorithm>
#include <new>

#include "Assert.hpp"

namespace Crowy
{
    void ComponentPool::FreeAligned::operator()(
        std::byte* rows
    ) const noexcept {
        ::operator delete(rows, std::align_val_t{align});
    }

    ComponentPool::~ComponentPool() {
        Clear();
    }

    ComponentPool::ComponentPool(const TypeOps& ops)
        : ops(ops), rows(nullptr, FreeAligned{.align = ops.align}) {
        CROWY_ASSERT(
            ops.moveConstruct != nullptr && ops.destroy != nullptr,
            "a component must move and destroy"
        );
    }

    void* ComponentPool::Insert(ActorId actor, void* value) {
        const auto slot = actor.GetIndex();
        if(slot >= rowOfActor.size())
            rowOfActor.resize(slot + 1, NoRow);
        CROWY_ASSERT(
            rowOfActor[slot] == NoRow,
            "an actor holds one row per type"
        );

        if(owners.size() == capacity)
            grow();

        auto* row = rowAt(owners.size());
        ops.moveConstruct(row, value);
        rowOfActor[slot] = static_cast<u32>(owners.size());
        owners.push_back(actor);

        return row;
    }

    void ComponentPool::Erase(ActorId actor) {
        const auto slot = actor.GetIndex();
        CROWY_ASSERT(Find(actor) != nullptr, "the actor has no row here");

        const auto row = rowOfActor[slot];
        const auto last = owners.size() - 1;

        ops.destroy(rowAt(row));
        if(row != last) {
            ops.moveConstruct(rowAt(row), rowAt(last));
            ops.destroy(rowAt(last));
            owners[row] = owners[last];
            rowOfActor[owners[row].GetIndex()] = row;
        }

        owners.pop_back();
        rowOfActor[slot] = NoRow;
    }

    void ComponentPool::Clear() {
        for(usize row = 0; row < owners.size(); ++row) {
            ops.destroy(rowAt(row));
            rowOfActor[owners[row].GetIndex()] = NoRow;
        }
        owners.clear();
    }

    void* ComponentPool::Find(ActorId actor) const noexcept {
        const auto slot = actor.GetIndex();
        if(slot >= rowOfActor.size() || rowOfActor[slot] == NoRow)
            return nullptr;

        const auto row = rowOfActor[slot];
        return owners[row] == actor ? rowAt(row) : nullptr;
    }

    void ComponentPool::grow() {
        const auto grown = std::max<usize>(capacity * 2, 16);
        Rows next(
            static_cast<std::byte*>(
                ::operator new(grown * ops.size, std::align_val_t{ops.align})
            ),
            FreeAligned{.align = ops.align}
        );

        for(usize row = 0; row < owners.size(); ++row) {
            auto* from = rowAt(row);
            ops.moveConstruct(next.get() + row * ops.size, from);
            ops.destroy(from);
        }

        rows = std::move(next);
        capacity = grown;
    }

    std::byte* ComponentPool::rowAt(usize row) const noexcept {
        return rows.get() + row * ops.size;
    }
}
