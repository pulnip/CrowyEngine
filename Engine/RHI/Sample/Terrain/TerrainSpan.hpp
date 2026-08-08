#pragma once

#include <span>
#include <vector>
#include "Terrain.hpp"

namespace Crowy
{
    // The span representation and the mesher over it. RHI-free, so it runs
    // without a device.

    // one solid run along a column's height axis: the y-axis RLE of the
    // occupancy function
    struct TerrainSpan{
        f32 yBottom, yTop;
    };

    struct TerrainSpanField{
        // indexed [z * TERRAIN_CELLS_X + x]
        // each column's runs are sorted by height and disjoint,
        // which the wall mesher relies on
        std::vector<std::vector<TerrainSpan>> columns;

        std::span<const TerrainSpan> At(u32 x, u32 z) const{
            return columns[z * TERRAIN_CELLS_X + x];
        }
        // outside the grid reads as air, so border columns emit outward walls
        std::span<const TerrainSpan> AtOrAir(i32 x, i32 z) const{
            const auto xOutOfCell = x < 0 || x >= static_cast<i32>(TERRAIN_CELLS_X);
            const auto zOutOfCell = z < 0 || z >= static_cast<i32>(TERRAIN_CELLS_Z);
            if(xOutOfCell || zOutOfCell)
                return {};

            return At(static_cast<u32>(x), static_cast<u32>(z));
        }
    };

    // Scans each column for zero crossings of the density, interpolated to
    // sub-cell precision.
    TerrainSpanField ExtractTerrainSpans(const TerrainParams&);

    struct TerrainSpanMeshStats{
        u32 columnCount = 0;
        u32 spanCount = 0;
        u32 maxSpansPerColumn = 0;
        u32 vertexCount = 0;
        // capacity ran out; the rest was dropped
        bool overflowed = false;
    };

    struct TerrainSpanMeshOptions{
        bool emitWalls = true;
    };

    // Fills `out` with a triangle soup.
    // stops at `capacityVertices` instead of growing - the GPU
    // buffer behind it is allocated once (the RHI has no deferred free).
    TerrainSpanMeshStats BuildTerrainSpanMesh(
        const TerrainSpanField&,
        std::vector<TerrainVertex>& out,
        u32 capacityVertices,
        TerrainSpanMeshOptions = {}
    );
}
