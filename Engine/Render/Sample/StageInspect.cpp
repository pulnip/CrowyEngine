#include "ClassRegistry.hpp"
#include "Object.hpp"
#include "StageDocument.hpp"

// The scene file's rows as the inspector edits them, in the file's units:
// meters and degrees. Names stay out: a renamed row would no longer be the
// one the selection names.
namespace Crowy
{
    // clang-format off: each registration opens a namespace the formatter
    // cannot see
    CROWY_STRUCT(StageInstance)
        .SetProperty("position", &StageInstance::position)
        .SetProperty("yaw", &StageInstance::yaw)
        .SetUIRange(0.0f, 360.0f)
        .SetProperty("scale", &StageInstance::scale)
    CROWY_STRUCT_END(StageInstance)

    CROWY_STRUCT(StageQuad)
        .SetProperty("position", &StageQuad::position)
        .SetProperty("yaw", &StageQuad::yaw)
        .SetUIRange(0.0f, 360.0f)
        .SetProperty("width", &StageQuad::width)
        .SetProperty("height", &StageQuad::height)
        .SetProperty("uv0", &StageQuad::uv0)
        .SetProperty("uv1", &StageQuad::uv1)
    CROWY_STRUCT_END(StageQuad)
    // clang-format on
}
