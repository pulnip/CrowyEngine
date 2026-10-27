#pragma once

#include <span>
#include <vector>

#include "Terrain.hpp"

namespace Crowy
{
    // one solid run along a column's height axis
    struct TerrainSpan {
        f32 yBottom, yTop;
    };

    struct TerrainSpanField {
        // each column's runs are sorted by height and disjoint,
        // which the wall mesher relies on
        std::vector<std::vector<TerrainSpan>> columns;

        std::span<const TerrainSpan> At(u32 x, u32 z) const {
            return columns[z * TERRAIN_CELLS_X + x];
        }
        // outside the grid reads as air, so border columns emit outward walls
        std::span<const TerrainSpan> AtOrAir(i32 x, i32 z) const {
            const auto xOutOfCell =
                x < 0 || x >= static_cast<i32>(TERRAIN_CELLS_X);
            const auto zOutOfCell =
                z < 0 || z >= static_cast<i32>(TERRAIN_CELLS_Z);
            if(xOutOfCell || zOutOfCell)
                return {};

            return At(static_cast<u32>(x), static_cast<u32>(z));
        }
    };

    TerrainSpanField ExtractTerrainSpans(const TerrainParams&);

    struct TerrainSpanMeshStats {
        u32 columnCount = 0;
        u32 spanCount = 0;
        u32 maxSpansPerColumn = 0;
        u32 vertexCount = 0;
        bool overflowed = false;
    };

    struct TerrainSpanMeshOptions {
        bool emitWalls = true;
    };

    TerrainSpanMeshStats BuildTerrainSpanMesh(
        const TerrainSpanField&,
        std::vector<TerrainVertex>& out,
        u32 capacityVertices,
        TerrainSpanMeshOptions = {}
    );
}
