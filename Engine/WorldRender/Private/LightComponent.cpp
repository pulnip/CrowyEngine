#include "LightComponent.hpp"

#include "Component.hpp"
#include "LinearAlgebra.hpp"
#include "RenderScene.hpp"
#include "System.hpp"
#include "World.hpp"

namespace Crowy
{
    namespace
    {
        void addRow(World& world, ActorId, LightComponent& light) {
            auto& lights = world.Service<RenderScene>().Lights();

            light.row = lights.Add(LightSnapshot{});
        }

        void removeRow(World& world, ActorId, LightComponent& light) {
            auto& lights = world.Service<RenderScene>().Lights();

            lights.Remove(light.row);
        }

        void extractLights(World& world, const FrameTime&) {
            // asked for on the first light, so a World without one needs no
            // RenderScene
            RenderScene* scene = nullptr;
            world.EachChanged<LightComponent>([&](ActorId actor,
                                                  const LightComponent& light) {
                if(scene == nullptr)
                    scene = &world.Service<RenderScene>();

                scene->Lights().GetRef(light.row) = LightSnapshot{
                    .kind = light.kind,
                    .enabled = light.enabled,
                    .castShadow = light.castShadow,
                    .color = light.color,
                    .intensity = light.intensity,
                    .position = world.WorldPosition(actor),
                    .direction = forward(world.WorldRotation(actor)),
                    .range = light.range,
                    .innerConeAngle = light.innerConeAngle,
                    .outerConeAngle = light.outerConeAngle,
                    .shadowBias = light.shadowBias,
                    .shadowNormalBias = light.shadowNormalBias
                };
            });
        }
    }

    // clang-format off
    CROWY_COMPONENT(LightComponent)
        .SetProperty("kind", &LightComponent::kind)
        .SetProperty("enabled", &LightComponent::enabled)
        .SetProperty("castShadow", &LightComponent::castShadow)
        .SetProperty("color", &LightComponent::color)
        .SetProperty("intensity", &LightComponent::intensity)
        .SetUIRange(0.0f, 16.0f)
        .SetProperty("range", &LightComponent::range)
        .SetUIRange(0.0f, 20.0f)
        .SetProperty("innerConeAngle", &LightComponent::innerConeAngle)
        .SetUIRange(0.0f, 1.57f)
        .SetProperty("outerConeAngle", &LightComponent::outerConeAngle)
        .SetUIRange(0.0f, 1.57f)
        .SetProperty("shadowBias", &LightComponent::shadowBias)
        .SetUIRange(0.0f, 0.1f)
        .SetProperty("shadowNormalBias", &LightComponent::shadowNormalBias)
        .SetUIRange(0.0f, 4.0f)
        .OnAdd(&addRow)
        .OnRemove(&removeRow)
    CROWY_COMPONENT_END(LightComponent)

    CROWY_SYSTEM(ExtractLights, Extract, &extractLights)
    CROWY_SYSTEM_END(ExtractLights)
    // clang-format on
}
