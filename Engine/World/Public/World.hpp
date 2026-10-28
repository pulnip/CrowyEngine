#pragma once

#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "Actor.hpp"
#include "Assert.hpp"
#include "ClassRegistry.hpp"
#include "Component.hpp"
#include "ComponentPool.hpp"
#include "ErasedValue.hpp"
#include "FixedTickClock.hpp"
#include "LinearAlgebra.hpp"
#include "Primitives.hpp"
#include "PropertyTarget.hpp"
#include "Semantics.hpp"
#include "System.hpp"
#include "TransformHierarchy.hpp"

namespace Crowy
{
    // The actors, their components and the systems over them. Structure lands
    // at the frame's Sync; a handle turns valid or invalid as the call returns.
    class World {
    private:
        static constexpr f64 TimeScale = 1.0;

        // one component type: its rows, what Sync still owes them, and what
        // changed since the last Extract
        struct ComponentStore {
            const ComponentInfo& info;

            ComponentPool rows;
            ComponentPool pendingAdds;

            std::vector<ActorId> pendingRemoves;
            // each actor once, in the order its row first changed
            std::vector<ActorId> changed;
            // by actor slot: the epoch it last joined `changed` in
            std::vector<u64> changedIn;

            ComponentStore(const ComponentInfo& info, const TypeOps& ops)
                : info(info), rows(ops), pendingAdds(ops) {}
        };

        using Stores = std::vector<std::unique_ptr<ComponentStore>>;
        using StoreOfType =
            std::unordered_map<const TypeDesc*, ComponentStore*>;
        using Services = std::unordered_map<std::type_index, void*>;

        TransformHierarchy hierarchy;
        std::vector<ActorId> dyingActors;
        // by the actor's slot; empty is no name
        std::vector<Str> names;
        // by the actor's slot
        std::vector<i64> fileIds;
        i64 nextFileId = 1;

        // in the order each type was first added, which Sync follows
        Stores stores;
        StoreOfType storeOfType;

        Services services;

        // a hook may not change structure while Sync applies it
        bool applyingStructure = false;
        // a row written while Extract runs would never be extracted
        bool extracting = false;

        // the changes since the last Extract; starts above changedIn's zero
        u64 epoch = 1;
        PhaseSchedule schedule;

        // the game's fixed clock, fed the game's time
        FixedTickClock fixedClock;
        u64 ticks = 0;
        u32 frameTicks = 0;

    public:
        ~World() = default;
        CROWY_DECLARE_PINNED(World)

        explicit World(
            const SystemRegistry& systems = SystemRegistry::Global()
        );

        ActorId Spawn(
            const Transform& local = Transform::Identity(),
            ActorId parent = ActorId::InvalidHandle()
        );
        // the actor and every actor below it
        void Destroy(ActorId actor);
        bool IsValid(ActorId actor) const noexcept;

        // what the actor is shown as, empty until set; two actors may share
        // one, so a reference names the actor itself
        const Str& Name(ActorId actor) const noexcept;
        void SetName(ActorId actor, Str name);

        // the id a scene file knows the actor by, given at its spawn; a load
        // keeps the file's, so an edit renumbers no other actor
        i64 FileId(ActorId actor) const noexcept;
        // unique by the caller, as a load keeps it; later spawns go above it
        void SetFileId(ActorId actor, i64 id);

        const Transform& LocalTransform(ActorId actor) const noexcept;
        void SetLocalTransform(ActorId actor, const Transform& local) noexcept;
        // as of the last Sync
        const Mat4& WorldMatrix(ActorId actor) const noexcept;
        Vec3 WorldPosition(ActorId actor) const noexcept;
        // the rotation of the TRS chain, which an uneven scale above the
        // actor leaves out of WorldMatrix's
        Vec4 WorldRotation(ActorId actor) const noexcept;

        // the actor's component of this type, found again at every access:
        // nullptr until the component's Sync, and once it or the actor is gone
        PropertyTarget Target(ActorId actor, const TypeDesc& desc);
        // the actor's local transform; a write moves it at the next Sync
        PropertyTarget TransformTarget(ActorId actor);

        template<typename T>
        PropertyTarget Target(ActorId actor) {
            return Target(actor, *getDesc<T>());
        }

        // moves the row out of `component`; its type is the value's own
        void Add(ActorId actor, ErasedValue& component);

        template<typename T>
        void Add(ActorId actor, T component) {
            addComponent(*getDesc<T>(), actor, &component);
        }

        template<typename T>
        void Remove(ActorId actor) {
            removeComponent(*getDesc<T>(), actor);
        }

        // nullptr until the component's Sync, and once it or its actor is gone
        template<typename T>
        const T* Get(ActorId actor) const noexcept {
            return static_cast<const T*>(findRow(*getDesc<T>(), actor));
        }

        // the only way to change a row, so the next Extract sees it
        template<typename T>
        T* Write(ActorId actor) noexcept {
            return static_cast<T*>(writeRow(*getDesc<T>(), actor));
        }

        // f(ActorId, const T&) for every row, in the order the rows landed
        template<typename T, typename F>
        void Each(F&& f) {
            const auto store = findStore(*getDesc<T>());
            if(store == nullptr)
                return;

            for(usize i = 0; i < store->rows.Count(); ++i) {
                auto id = store->rows.OwnerAt(i);
                if(!IsValid(id))
                    continue;

                f(id, *static_cast<const T*>(store->rows.At(i)));
            }
        }

        // f(ActorId, const T&) for every row written, landed or moved since
        // the last Extract, each once; empty after Extract
        template<typename T, typename F>
        void EachChanged(F&& f) {
            const auto store = findStore(*getDesc<T>());
            if(store == nullptr)
                return;

            for(const auto actor: store->changed) {
                // removed, or its actor destroyed, after it changed
                if(auto row = static_cast<const T*>(store->rows.Find(actor)))
                    f(actor, *row);
            }
        }

        // f(ActorId actor, ActorId parent) for every actor, a parent before
        // its children; a root's parent is invalid
        template<typename F>
        void EachActor(F&& f) const {
            // an unplaced spawn would be missed and a pending destroy walked
            CROWY_ASSERT(
                !hasPendingStructure(),
                "walking the actors before Sync applied a change"
            );

            for(usize i = 0; i < hierarchy.Size(); ++i) {
                const auto node = hierarchy.HandleAt(i);
                f(actorOf(node), actorOf(hierarchy.GetParent(node)));
            }
        }

        // f(const ComponentInfo&, const void* row) in the order this World
        // first met each type, which a file written in it would not read back
        template<typename F>
        void EachComponent(ActorId actor, F&& f) const {
            CROWY_ASSERT(
                !hasPendingStructure(),
                "reading the components before Sync applied a change"
            );

            for(const auto& store: stores) {
                if(const void* row = store->rows.Find(actor))
                    f(store->info, row);
            }
        }

        // what a system or a hook reaches outside the World, by type
        template<typename T>
        void Provide(T& service) {
            const auto idx = std::type_index(typeid(T));
            services[idx] = &service;
        }

        template<typename T>
        T& Service() const noexcept {
            const auto idx = std::type_index(typeid(T));
            const auto it = services.find(idx);
            CROWY_ASSERT(it != services.end(), "no such service was provided");

            return *static_cast<T*>(it->second);
        }

        // runs every Phase in order, the FixedUpdate ones once per fixed tick
        // the game clock owes
        void Frame(f64 realSeconds);

        // for a reader outside the phases, such as a GPU simulation recorded
        // after the frame: the fixed ticks so far, and the last Frame's
        u64 Ticks() const noexcept { return ticks; }
        u32 FrameTicks() const noexcept { return frameTicks; }

    private:
        static TransformHandle nodeOf(ActorId actor) noexcept;
        static ActorId actorOf(TransformHandle node) noexcept;

        void addComponent(const TypeDesc& desc, ActorId actor, void* component);
        void removeComponent(const TypeDesc& desc, ActorId actor);
        ComponentStore& storeFor(const TypeDesc& desc);
        ComponentStore* findStore(const TypeDesc& desc) const noexcept;
        const void* findRow(const TypeDesc& desc, ActorId actor) const noexcept;
        // nullptr when no actor has had the component yet
        void* writeRow(const TypeDesc& desc, ActorId actor) noexcept;
        void markChanged(ComponentStore& store, ActorId actor);
        bool hasPendingStructure() const noexcept;

        void run(Phase phase, const FrameTime& time);
        void sync(const FrameTime& time);
    };
}
