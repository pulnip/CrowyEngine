#include "ModelActors.hpp"

#include "Assert.hpp"

namespace Crowy
{
    std::vector<ActorId> spawnModelActors(
        World& world,
        const ModelData& model,
        ActorId parent
    ) {
        std::vector<ActorId> actors;
        actors.reserve(model.nodes.size());
        for(const auto& node: model.nodes) {
            CROWY_ASSERT(
                node.parent == ModelNode::NoParent ||
                node.parent < actors.size()
            );

            const auto actor = world.Spawn(
                node.local,
                node.parent == ModelNode::NoParent ? parent
                                                   : actors[node.parent]
            );
            world.SetName(actor, node.name);
            actors.push_back(actor);
        }

        return actors;
    }
}
