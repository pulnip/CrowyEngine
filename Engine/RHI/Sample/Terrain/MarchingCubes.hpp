#pragma once

#include <span>

#include "Primitives.hpp"

namespace Crowy
{
    // The Lorensen / Bourke table; case bit i is set when corner i is solid,
    // so its winding puts the right-hand normal (the front face) toward air.

    inline constexpr u32 MC_CASE_COUNT = 256;
    inline constexpr u32 MC_MAX_INDICES = 16;

    inline constexpr u32 MC_EDGE_CORNERS[12][2]{
        {0, 1},
        {1, 2},
        {2, 3},
        {3, 0},
        {4, 5},
        {5, 6},
        {6, 7},
        {7, 4},
        {0, 4},
        {1, 5},
        {2, 6},
        {3, 7}
    };

    inline constexpr u32 MC_CORNER_OFFSET[8][3]{
        {0, 0, 0},
        {1, 0, 0},
        {1, 0, 1},
        {0, 0, 1},
        {0, 1, 0},
        {1, 1, 0},
        {1, 1, 1},
        {0, 1, 1}
    };

    std::span<const i32> MarchingCubesTriTable();
}
