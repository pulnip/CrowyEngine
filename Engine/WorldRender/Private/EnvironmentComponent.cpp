#include "EnvironmentComponent.hpp"

#include <source_location>

#include "Assert.hpp"
#include "Component.hpp"
#include "RenderScene.hpp"
#include "System.hpp"
#include "World.hpp"

namespace Crowy
{
    namespace
    {
        // two environments would leave the scene to whichever changed last
        void requireOneEnvironment(
            World& world,
            ActorId,
            EnvironmentComponent&
        ) {
            usize environments = 0;
            world.Each<EnvironmentComponent>(
                [&](ActorId, const EnvironmentComponent&) { ++environments; }
            );

            if(environments > 1) {
                detail::assertFail(
                    "one environment",
                    std::source_location::current(),
                    "a World has one environment"
                );
            }
        }

        void resetEnvironment(World& world, ActorId, EnvironmentComponent&) {
            world.Service<RenderScene>().Environment() = EnvironmentSnapshot{};
        }

        void extractEnvironment(World& world, const FrameTime&) {
            world.EachChanged<EnvironmentComponent>(
                [&](ActorId, const EnvironmentComponent& environment) {
                    world.Service<RenderScene>().Environment() =
                        EnvironmentSnapshot{
                            .skyAmbient = environment.skyAmbient,
                            .groundAmbient = environment.groundAmbient,
                            .sky = environment.skyColor
                    };
                }
            );
        }
    }

    // clang-format off
    CROWY_COMPONENT(EnvironmentComponent)
        .AddProperty("skyAmbient", &EnvironmentComponent::skyAmbient)
        .AddProperty("groundAmbient", &EnvironmentComponent::groundAmbient)
        .AddProperty("skyColor", &EnvironmentComponent::skyColor)
        .OnAdd(&requireOneEnvironment)
        .OnRemove(&resetEnvironment)
    CROWY_COMPONENT_END(EnvironmentComponent)

    CROWY_SYSTEM(ExtractEnvironment, Extract, &extractEnvironment)
    CROWY_SYSTEM_END(ExtractEnvironment)
    // clang-format on
}
