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
#include "FixedTickClock.hpp"
#include "LinearAlgebra.hpp"
#include "Primitives.hpp"
#include "PropertyTarget.hpp"
#include "Semantics.hpp"
#include "System.hpp"
#include "TransformHierarchy.hpp"

namespace Crowy
{
    // The runtime the actors live in: their transforms, their components, the
    // systems run over them in the fixed phases, and the game's clocks.
    // An actor is its transform node. A structural change (a spawn, a
    // destroy, a component added or removed) lands at the frame's Sync, never
    // while a phase iterates; a handle turns valid, or invalid, the moment the
    // call returns. What changed (a row written, landed or moved) is recorded,
    // so a frame where nothing changed recomputes and extracts nothing.
    class World {
    private:
        // the game clock runs at the real one's rate until a consumer scales it
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
            return Target(actor, *GetDesc<T>());
        }

        template<typename T>
        void Add(ActorId actor, T component) {
            addComponent(*GetDesc<T>(), actor, &component);
        }

        template<typename T>
        void Remove(ActorId actor) {
            removeComponent(*GetDesc<T>(), actor);
        }

        // nullptr until the component's Sync, and after it is removed
        template<typename T>
        const T* Get(ActorId actor) const noexcept {
            return static_cast<const T*>(findRow(*GetDesc<T>(), actor));
        }

        // the only way to change a row, so the next Extract sees it
        template<typename T>
        T* Write(ActorId actor) noexcept {
            return static_cast<T*>(writeRow(*GetDesc<T>(), actor));
        }

        // f(ActorId, const T&) for every row, in the order the rows landed
        template<typename T, typename F>
        void Each(F&& f) {
            const auto store = findStore(*GetDesc<T>());
            if(store == nullptr)
                return;

            for(usize i = 0; i < store->rows.Count(); ++i){
                auto id = store->rows.OwnerAt(i);
                auto row = static_cast<const T*>(store->rows.At(i));

                f(id, *row);
            }
        }

        // f(ActorId, const T&) for every row written, landed or moved since
        // the last Extract, each once; empty after Extract
        template<typename T, typename F>
        void EachChanged(F&& f) {
            const auto store = findStore(*GetDesc<T>());
            if(store == nullptr)
                return;

            for(const auto actor: store->changed) {
                // removed, or its actor destroyed, after it changed
                if(auto row = static_cast<const T*>(store->rows.Find(actor)))
                    f(actor, *row);
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

        // Input, the fixed ticks the game clock owes (PreFixedUpdate, Physics,
        // PostFixedUpdate each), Update, Sync, Extract
        void Frame(f64 realSeconds);

        // what a reader outside the phases, such as a GPU simulation
        // recorded after the frame, steps by: the fixed ticks run so far,
        // and those the last Frame ran
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

        void run(Phase phase, const FrameTime& time);
        void sync(const FrameTime& time);
    };
}
