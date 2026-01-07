#include "EntityRegistry.hpp"
#include "ECSSystem.hpp"

namespace Crowy
{
    void RenderSystem::update(EntityRegistry& registry, UpdateContext& ctx){
        for(auto [id, bit, transform, renderObj]:
            registry.query<TransformComponent, RenderObjectComponent>()
        ){
        }
    }
}