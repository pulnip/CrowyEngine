#pragma once

#include "EnumUtil.hpp"
#include "Primitives.hpp"
#include "RenderEnums.h"

namespace Crowy
{
    // what a view shows instead of the plain lit picture, carried as
    // ViewData::debugMode
    enum class DebugMode : u32 { CROWY_DEBUG_MODES(CROWY_ENUMERATOR) };

    // clang-format off
    CROWY_ENUM_BEGIN(DebugMode)
        CROWY_DEBUG_MODES(CROWY_ENUM_ENTRY)
    CROWY_ENUM_END()
    // clang-format on
}
