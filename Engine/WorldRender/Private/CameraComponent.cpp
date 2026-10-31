#include "CameraComponent.hpp"

#include "Component.hpp"
#include "RenderCamera.hpp"
#include "System.hpp"
#include "World.hpp"

namespace Crowy
{
    namespace
    {
        ActorId activeCamera(World& world) {
            auto active = ActorId::InvalidHandle();
            world.Each<CameraComponent>([&](ActorId actor,
                                            const CameraComponent&) {
                if(!active.IsValid() ||
                   world.FileId(actor) < world.FileId(active)) {
                    active = actor;
                }
            });

            return active;
        }

        void extractCamera(World& world, const FrameTime&) {
            // the lowest FileId, found once a camera has changed
            auto active = ActorId::InvalidHandle();
            world.EachChanged<CameraComponent>(
                [&](ActorId actor, const CameraComponent& camera) {
                    if(!active.IsValid())
                        active = activeCamera(world);
                    if(actor != active)
                        return;

                    const auto position = world.WorldPosition(actor);

                    world.Service<CameraSnapshot>() = CameraSnapshot{
                        .view = viewMat(position, world.WorldRotation(actor)),
                        .position = position,
                        .fovY = camera.fovY,
                        .nearZ = camera.nearZ,
                        .farZ = camera.farZ
                    };
                }
            );
        }
    }

    // clang-format off
    CROWY_COMPONENT(CameraComponent)
        .AddProperty("fovY", &CameraComponent::fovY)
        .SetUIRange(0.35f, 2.4f)
        .AddProperty("nearZ", &CameraComponent::nearZ)
        .AddProperty("farZ", &CameraComponent::farZ)
    CROWY_COMPONENT_END(CameraComponent)

    CROWY_SYSTEM(ExtractCamera, Extract, &extractCamera)
    CROWY_SYSTEM_END(ExtractCamera)
    // clang-format on
}
