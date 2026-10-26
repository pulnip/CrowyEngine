#include "EffectComponent.hpp"

#include "Component.hpp"
#include "EffectSystem.hpp"
#include "System.hpp"
#include "World.hpp"

namespace Crowy
{
    namespace
    {
        // a name no effect has throws here, where the actor gets it
        void requireEffect(World& world, ActorId, EffectComponent& component) {
            world.Service<EffectSystem>().Origin(component.effect);
        }

        void extractEffects(World& world, const FrameTime&) {
            // asked for on the first changed row, so a World without one
            // needs no EffectSystem
            EffectSystem* effects = nullptr;
            world.EachChanged<EffectComponent>(
                [&](ActorId actor, const EffectComponent& component) {
                    if(effects == nullptr)
                        effects = &world.Service<EffectSystem>();

                    effects->SetOrigin(
                        component.effect,
                        world.WorldPosition(actor)
                    );
                }
            );
        }
    }

    // clang-format off
    CROWY_COMPONENT(EffectComponent)
        .AddProperty("effect", &EffectComponent::effect)
        .OnAdd(&requireEffect)
    CROWY_COMPONENT_END(EffectComponent)

    CROWY_SYSTEM(ExtractEffects, Extract, &extractEffects)
    CROWY_SYSTEM_END(ExtractEffects)
    // clang-format on
}
