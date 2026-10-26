#include "World.hpp"

#include <algorithm>

#include "ClassRegistry.hpp"

namespace Crowy
{
    World::World(const SystemRegistry& systems)
        : schedule(scheduleSystems(systems.All())) {}

    ActorId World::Spawn(const Transform& local, ActorId parent) {
        CROWY_ASSERT(!applyingStructure, "a spawn inside Sync");

        const auto actor = actorOf(
            parent.IsValid() ? hierarchy.CreateNode(nodeOf(parent), local)
                             : hierarchy.CreateNode(local)
        );

        const auto slot = actor.GetIndex();
        if(slot >= fileIds.size())
            fileIds.resize(slot + 1);
        fileIds[slot] = nextFileId++;

        return actor;
    }

    void World::Destroy(ActorId actor) {
        CROWY_ASSERT(!applyingStructure, "a destroy inside Sync");
        CROWY_ASSERT(IsValid(actor), "destroying an actor that is not alive");

        std::vector<TransformHandle> destroyed;
        hierarchy.DestroySubtree(nodeOf(actor), destroyed);

        for(const auto node: destroyed)
            dyingActors.push_back(actorOf(node));
    }

    bool World::IsValid(ActorId actor) const noexcept {
        return hierarchy.IsValid(nodeOf(actor));
    }

    const Str& World::Name(ActorId actor) const noexcept {
        static const Str unnamed;
        CROWY_ASSERT(IsValid(actor), "naming an actor that is not alive");

        const auto slot = actor.GetIndex();
        return slot < names.size() ? names[slot] : unnamed;
    }

    void World::SetName(ActorId actor, Str name) {
        CROWY_ASSERT(IsValid(actor), "naming an actor that is not alive");

        const auto slot = actor.GetIndex();
        if(slot >= names.size())
            names.resize(slot + 1);
        names[slot] = std::move(name);
    }

    i64 World::FileId(ActorId actor) const noexcept {
        CROWY_ASSERT(IsValid(actor), "reading the id of an actor not alive");

        return fileIds[actor.GetIndex()];
    }

    void World::SetFileId(ActorId actor, i64 id) {
        CROWY_ASSERT(IsValid(actor), "giving an id to an actor not alive");
        CROWY_ASSERT(id >= 1, "a file id starts at 1");

        fileIds[actor.GetIndex()] = id;
        nextFileId = std::max(nextFileId, id + 1);
    }

    const Transform& World::LocalTransform(ActorId actor) const noexcept {
        return hierarchy.GetLocalTransform(nodeOf(actor));
    }

    void World::SetLocalTransform(
        ActorId actor,
        const Transform& local
    ) noexcept {
        hierarchy.SetLocalTransform(nodeOf(actor), local);
    }

    const Mat4& World::WorldMatrix(ActorId actor) const noexcept {
        return hierarchy.GetWorldMatrix(nodeOf(actor));
    }

    Vec3 World::WorldPosition(ActorId actor) const noexcept {
        return hierarchy.GetWorldPosition(nodeOf(actor));
    }

    Vec4 World::WorldRotation(ActorId actor) const noexcept {
        return hierarchy.GetWorldRotation(nodeOf(actor));
    }

    PropertyTarget World::Target(ActorId actor, const TypeDesc& desc) {
        CROWY_ASSERT(
            ComponentRegistry::Get().Find(desc) != nullptr,
            "a type that is not a component"
        );

        // a destroyed actor keeps its rows until Sync, so validity comes first
        return PropertyTarget(
            [this, actor, &desc]() -> const void* {
                return IsValid(actor) ? findRow(desc, actor) : nullptr;
            },
            [this, actor, &desc]() -> void* {
                if(!IsValid(actor) || findRow(desc, actor) == nullptr)
                    return nullptr;

                return writeRow(desc, actor);
            },
            desc
        );
    }

    PropertyTarget World::TransformTarget(ActorId actor) {
        return PropertyTarget(
            [this, actor]() -> const void* {
                return IsValid(actor)
                           ? &hierarchy.GetLocalTransform(nodeOf(actor))
                           : nullptr;
            },
            [this, actor]() -> void* {
                return IsValid(actor)
                           ? &hierarchy.WriteLocalTransform(nodeOf(actor))
                           : nullptr;
            },
            *getDesc<Transform>()
        );
    }

    void World::Frame(f64 realSeconds) {
        const auto gameSeconds = realSeconds * TimeScale;
        const FrameTime frame{
            .realSeconds = realSeconds,
            .gameSeconds = gameSeconds,
            .tick = ticks
        };
        run(Phase::Input, frame);

        const auto due = fixedClock.Advance(toNanoseconds(gameSeconds));
        frameTicks = due;
        for(u32 i = 0; i < due; ++i) {
            const FrameTime tick{
                .realSeconds = realSeconds,
                .gameSeconds = static_cast<f64>(TickNanoseconds) * 1e-9,
                .tick = ++ticks
            };
            run(Phase::PreFixedUpdate, tick);
            run(Phase::Physics, tick);
            run(Phase::PostFixedUpdate, tick);
        }

        const FrameTime ticked{
            .realSeconds = realSeconds,
            .gameSeconds = gameSeconds,
            .tick = ticks
        };
        run(Phase::Update, ticked);
        sync(ticked);

        extracting = true;
        run(Phase::Extract, ticked);
        extracting = false;

        // what changed is now extracted; the next frame records afresh
        for(const auto& store: stores)
            store->changed.clear();
        ++epoch;
    }

    TransformHandle World::nodeOf(ActorId actor) noexcept {
        return {actor.GetIndex(), actor.GetGeneration()};
    }

    ActorId World::actorOf(TransformHandle node) noexcept {
        return {node.GetIndex(), node.GetGeneration()};
    }

    void World::Add(ActorId actor, ErasedValue& component) {
        const auto& ops = component.Ops();
        CROWY_ASSERT(ops.getDesc != nullptr, "a type that is not a component");
        addComponent(*ops.getDesc(), actor, component.Get());
    }

    void World::addComponent(
        const TypeDesc& desc,
        ActorId actor,
        void* component
    ) {
        CROWY_ASSERT(!applyingStructure, "a component added inside Sync");
        CROWY_ASSERT(IsValid(actor), "adding to an actor that is not alive");

        auto& store = storeFor(desc);
        CROWY_ASSERT(
            store.rows.Find(actor) == nullptr &&
                store.pendingAdds.Find(actor) == nullptr,
            "the actor already has this component"
        );

        store.pendingAdds.Insert(actor, component);
    }

    void World::removeComponent(const TypeDesc& desc, ActorId actor) {
        CROWY_ASSERT(!applyingStructure, "a component removed inside Sync");
        CROWY_ASSERT(
            IsValid(actor),
            "removing from an actor that is not alive"
        );

        auto* store = findStore(desc);
        CROWY_ASSERT(
            store != nullptr && store->rows.Find(actor) != nullptr,
            "the actor has no such component to remove"
        );

        store->pendingRemoves.push_back(actor);
    }

    World::ComponentStore& World::storeFor(const TypeDesc& desc) {
        if(auto* store = findStore(desc))
            return *store;

        // getDesc made an empty desc: the registration was never linked
        CROWY_ASSERT(!desc.name.empty(), "a type that was never registered");
        const auto* info = ComponentRegistry::Get().Find(desc);
        CROWY_ASSERT(info != nullptr, "a type that is not a component");

        auto& store = stores.emplace_back(
            std::make_unique<ComponentStore>(*info, *desc.ops)
        );
        storeOfType.emplace(&desc, store.get());

        return *store;
    }

    World::ComponentStore* World::findStore(
        const TypeDesc& desc
    ) const noexcept {
        const auto it = storeOfType.find(&desc);
        return it == storeOfType.end() ? nullptr : it->second;
    }

    const void* World::findRow(
        const TypeDesc& desc,
        ActorId actor
    ) const noexcept {
        const auto* store = findStore(desc);
        if(store == nullptr)
            return nullptr;

        auto row = store->rows.Find(actor);
        return row;
    }

    void* World::writeRow(const TypeDesc& desc, ActorId actor) noexcept {
        auto store = findStore(desc);
        if(store == nullptr)
            return nullptr;

        auto row = store->rows.Find(actor);
        CROWY_ASSERT(row != nullptr, "the actor has no such component");

        markChanged(*store, actor);
        return row;
    }

    void World::markChanged(ComponentStore& store, ActorId actor) {
        CROWY_ASSERT(!extracting, "a component written inside Extract");

        const auto slot = actor.GetIndex();
        if(slot >= store.changedIn.size())
            store.changedIn.resize(slot + 1, 0);

        if(store.changedIn[slot] != epoch) {
            store.changedIn[slot] = epoch;
            store.changed.push_back(actor);
        }
    }

    bool World::hasPendingStructure() const noexcept {
        if(!dyingActors.empty() || hierarchy.HasPendingChanges())
            return true;

        for(const auto& store: stores) {
            if(store->pendingAdds.Count() != 0 ||
               !store->pendingRemoves.empty())
                return true;
        }
        return false;
    }

    void World::run(Phase phase, const FrameTime& time) {
        for(const auto system: schedule[static_cast<usize>(phase)])
            system(*this, time);
    }

    void World::sync(const FrameTime& time) {
        applyingStructure = true;

        // the dying first, so nothing below adds to an actor that is gone
        for(const auto actor: dyingActors) {
            // the slot is released below, and its next actor starts unnamed
            if(actor.GetIndex() < names.size())
                names[actor.GetIndex()].clear();

            for(const auto& store: stores) {
                if(auto* row = store->rows.Find(actor)) {
                    if(store->info.onRemove)
                        store->info.onRemove(*this, actor, row);
                    store->rows.Erase(actor);
                }
                if(store->pendingAdds.Find(actor) != nullptr)
                    store->pendingAdds.Erase(actor);
            }
        }
        dyingActors.clear();

        for(const auto& store: stores) {
            for(const auto actor: store->pendingRemoves) {
                auto* row = store->rows.Find(actor);
                if(row == nullptr)
                    continue;

                if(store->info.onRemove)
                    store->info.onRemove(*this, actor, row);
                store->rows.Erase(actor);
            }
            store->pendingRemoves.clear();
        }

        // the dying actors' slots are released here, after their rows left
        hierarchy.CommitStructuralChanges();

        for(const auto& store: stores) {
            for(usize i = 0; i < store->pendingAdds.Count(); ++i) {
                const auto actor = store->pendingAdds.OwnerAt(i);
                auto* row = store->rows.Insert(actor, store->pendingAdds.At(i));
                if(store->info.onAdd)
                    store->info.onAdd(*this, actor, row);
                markChanged(*store, actor);
            }
            store->pendingAdds.Clear();
        }

        applyingStructure = false;

        run(Phase::Sync, time);

        // only the marked subtrees, and every row riding on them changed
        hierarchy.UpdateWorldTransforms();
        for(const auto node: hierarchy.GetUpdatedNodes()) {
            const auto actor = actorOf(node);
            for(const auto& store: stores) {
                if(store->rows.Find(actor) != nullptr)
                    markChanged(*store, actor);
            }
        }
    }

    // here rather than in a file of its own: every World user links this one
    // clang-format off
    CROWY_STRUCT(Transform)
        .AddProperty("position", &Transform::position)
        .AddProperty("rotation", &Transform::rotation)
        .AddProperty("scale", &Transform::scale)
    CROWY_STRUCT_END(Transform)
    // clang-format on
}
