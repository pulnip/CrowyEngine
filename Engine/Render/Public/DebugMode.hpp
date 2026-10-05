#pragma once

#include "EnumUtil.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    // what a view shows instead of the plain lit picture, carried as
    // ViewData::debugMode; mirrored by the constants in
    // Engine/Shader/DebugView.slang
    enum class DebugMode : u32 {
        Lit,
        Unshaded,
        Normals,
        Depth,
        Overdraw,
        // the lights' shadow term: 1 white, 0 black
        Shadow,
    };

    // clang-format off: the formatter cannot see the specialization the
    // macros open and close
    CROWY_ENUM_BEGIN(DebugMode)
        CROWY_ENUM_VALUE(Lit)
        CROWY_ENUM_VALUE(Unshaded)
        CROWY_ENUM_VALUE(Normals)
        CROWY_ENUM_VALUE(Depth)
        CROWY_ENUM_VALUE(Overdraw)
        CROWY_ENUM_VALUE(Shadow)
    CROWY_ENUM_END()
    // clang-format on
}
