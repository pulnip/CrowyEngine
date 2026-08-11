#pragma once

#include <span>
#include "Primitives.hpp"

namespace Crowy
{
    // The standard marching cubes triangle table (Lorensen / Paul Bourke).
    //
    // A cell's eight corners are numbered
    //
    //        4--------5          +Y
    //       /|       /|          |
    //      / |      / |          |
    //     7--------6  |          o---- +X
    //     |  0-----|--1         /
    //     | /      | /        +Z
    //     |/       |/
    //     3--------2
    //
    // corner i sits at CORNER_OFFSET[i] cell-sizes from the cell's origin, and
    // the case index sets bit i when that corner is solid (density > 0). Each
    // row lists up to five triangles as edge indices, terminated by -1.
    //
    // With the solid-is-set convention the table's winding already puts the
    // right-hand normal on the air side, which is the front face this engine
    // rasterizes (left-handed, frontCounterClockwise = false).

    inline constexpr u32 MC_CASE_COUNT = 256;
    inline constexpr u32 MC_MAX_INDICES = 16;

    // the two corners each edge runs between
    inline constexpr u32 MC_EDGE_CORNERS[12][2]{
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };

    inline constexpr u32 MC_CORNER_OFFSET[8][3]{
        {0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1},
        {0, 1, 0}, {1, 1, 0}, {1, 1, 1}, {0, 1, 1}
    };

    // MC_CASE_COUNT * MC_MAX_INDICES entries, uploaded as a structured buffer
    std::span<const i32> MarchingCubesTriTable();
}
