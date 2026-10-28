#pragma once

#include <span>

#include "MarchingCubes.h"
#include "Primitives.hpp"

namespace Crowy
{
    // The Lorensen / Bourke table; case bit i is set when corner i is solid,
    // so its winding puts the right-hand normal (the front face) toward air.

    inline constexpr u32 MC_CASE_COUNT = 256;
    inline constexpr u32 MC_EDGE_CORNERS[12][2]{MC_EDGE_ENDS};
    inline constexpr u32 MC_CORNER_OFFSET[8][3]{MC_CORNER_OFFSETS};

    std::span<const i32> MarchingCubesTriTable();
}
