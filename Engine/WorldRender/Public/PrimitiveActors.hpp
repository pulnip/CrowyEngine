#pragma once

#include <optional>

#include "HandleMap.hpp"
#include "RenderScene.hpp"
#include "World.hpp"

namespace Crowy
{
    // the row the actor's MeshComponent or MeshRowComponent draws, once its
    // Sync built it
    std::optional<PrimitiveHandle> primitiveOfActor(
        const World& world,
        ActorId actor
    );

    // Which actor each primitive row draws, kept as the mesh components make
    // and drop their rows in a World that provides it.
    class PrimitiveActors {
    private:
        HandleMap<PrimitiveHandle, ActorId> actors;

    public:
        void Bind(PrimitiveHandle primitive, ActorId actor) {
            actors.Set(primitive, actor);
        }
        void Unbind(PrimitiveHandle primitive) { actors.Erase(primitive); }
        std::optional<ActorId> ActorOf(PrimitiveHandle primitive) const {
            return actors.Find(primitive);
        }
    };
}
