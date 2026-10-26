#include "CameraComponent.hpp"

#include <source_location>

#include "Assert.hpp"
#include "Component.hpp"
#include "RenderCamera.hpp"
#include "System.hpp"
#include "World.hpp"

namespace Crowy
{
    namespace
    {
        // two cameras would leave the snapshot to whichever changed last
        void requireOneCamera(World& world, ActorId, CameraComponent&) {
            usize cameras = 0;
            world.Each<CameraComponent>([&](ActorId, const CameraComponent&) {
                ++cameras;
            });

            if(cameras > 1) {
                detail::assertFail(
                    "one camera",
                    std::source_location::current(),
                    "a World is seen through one camera"
                );
            }
        }

        void extractCamera(World& world, const FrameTime&) {
            world.EachChanged<CameraComponent>(
                [&](ActorId actor, const CameraComponent& camera) {
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
        .SetProperty("fovY", &CameraComponent::fovY)
        .SetUIRange(0.35f, 2.4f)
        .SetProperty("nearZ", &CameraComponent::nearZ)
        .SetProperty("farZ", &CameraComponent::farZ)
        .OnAdd(&requireOneCamera)
    CROWY_COMPONENT_END(CameraComponent)

    CROWY_SYSTEM(ExtractCamera, Extract, &extractCamera)
    CROWY_SYSTEM_END(ExtractCamera)
    // clang-format on
}
