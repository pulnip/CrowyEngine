#include "PaintLabControl.hpp"

#include "ClassRegistry.hpp"
#include "FlyCamera.hpp"
#include "Object.hpp"

namespace Crowy
{
    // clang-format off: each registration opens a namespace the formatter
    // cannot see
    CROWY_STRUCT(PaintLabSettings)
        .SetProperty("view", &PaintLabSettings::view)
        .SetProperty("compareView", &PaintLabSettings::compareView)
        .SetProperty("split", &PaintLabSettings::split)
        .SetUIRange(0.0f, 1.0f)
        .SetProperty("stage", &PaintLabSettings::stage)
        .SetProperty("panel", &PaintLabSettings::panel)
        .SetProperty("panelChannel", &PaintLabSettings::panelChannel)
        .SetProperty("selected", &PaintLabSettings::selected)
        .SetProperty("labels", &PaintLabSettings::labels)
    CROWY_STRUCT_END(PaintLabSettings)

    CROWY_STRUCT(PaintObjectFlags)
        .SetProperty("front", &PaintObjectFlags::front)
        .SetProperty("back", &PaintObjectFlags::back)
        .SetProperty("right", &PaintObjectFlags::right)
        .SetProperty("left", &PaintObjectFlags::left)
        .SetProperty("up", &PaintObjectFlags::up)
        .SetProperty("down", &PaintObjectFlags::down)
        .SetProperty("floorFollowsWorldUp", &PaintObjectFlags::floorFollowsWorldUp)
    CROWY_STRUCT_END(PaintObjectFlags)

    CROWY_STRUCT(FlyCamera)
        .SetProperty("position", &FlyCamera::position)
        .SetProperty("yaw", &FlyCamera::yaw)
        .SetProperty("pitch", &FlyCamera::pitch)
        .SetUIRange(-1.55f, 1.55f)
        .SetProperty("fovY", &FlyCamera::config, &FlyCamera::Config::fovY)
        .SetUIRange(0.35f, 2.4f)
    CROWY_STRUCT_END(FlyCamera)
    // clang-format on
}
