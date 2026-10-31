#include "PrimitiveActors.hpp"

#include "MeshComponent.hpp"
#include "MeshRowComponent.hpp"

namespace Crowy
{
    std::optional<PrimitiveHandle> primitiveOfActor(
        const World& world,
        ActorId actor
    ) {
        if(const auto* mesh = world.Get<MeshComponent>(actor);
           mesh != nullptr && mesh->row.IsValid())
            return mesh->row;
        if(const auto* mesh = world.Get<MeshRowComponent>(actor);
           mesh != nullptr && mesh->row.IsValid())
            return mesh->row;

        return std::nullopt;
    }
}
