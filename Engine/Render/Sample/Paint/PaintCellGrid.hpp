#pragma once

#include <array>
#include <functional>
#include <span>
#include <vector>

#include "MintFrame.hpp"
#include "PaintTypes.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    // how much surface each paint id owns, in world cm^2; the PaintIdNone
    // entry is the unpainted area
    struct PaintCoverage {
        std::array<f32, PaintIdCount> areaByPaintId{};
        f32 totalArea = 0.0f;

        f32 Fraction(u8 paintId) const noexcept {
            return paintId >= PaintIdCount || totalArea <= 0.0f
                       ? 0.0f
                       : areaByPaintId[paintId] / totalArea;
        }

        void Add(const PaintCoverage& other) noexcept {
            for(usize id = 0; id < PaintIdCount; ++id)
                areaByPaintId[id] += other.areaByPaintId[id];
            totalArea += other.totalArea;
        }
    };

    // MintChoco's score cell edge, in world cm
    inline constexpr f32 PaintScoreCellSize = 25.0f;
    // the share of the stamp radius that claims a cell: the stamp's main
    // blob spans half the radius, its satellites almost all of it
    inline constexpr f32 PaintCellStampFraction = 0.5f;

    // FPaintCellGrid: who owns the surface, as the gameplay layer counts it.
    // A coarse voxel grid over the scaled-local bounds where every voxel keeps
    // one cell per face direction, its true triangle area deposited once.
    // A splat marks the cells its stamp covers; nothing here ever reads the
    // render target.
    class PaintCellGrid {
    private:
        DVec3 origin;
        f32 cellSize = PaintScoreCellSize;
        std::array<i32, 3> dims{};
        // all indexed by voxel * PaintFaceDirectionCount + direction; a zero
        // area means no surface
        std::vector<u8> ids;
        std::vector<u8> starGens;
        std::vector<f32> areas;
        // where the surface sits inside the voxel, area-weighted
        std::vector<Vec3> surfaceCenters;
        // kept as cells change, so a coverage query never walks them
        f32 totals[PaintFaceDirectionCount][PaintIdCount]{};
        f32 totalArea = 0.0f;
        i32 surfaceCellCount = 0;

    public:
        // `positions` are scaled-local; a triangle's direction is classified
        // from its geometric normal times normalClassifyScale, which undoes the
        // squash the scale puts on it
        void Build(
            const Box3d& bounds,
            f32 cellSize,
            std::span<const Vec3> positions,
            std::span<const Vec3> normals,
            std::span<const u32> indices,
            u8 enabledDirections = PaintAllDirectionsMask,
            DVec3 normalClassifyScale = {1.0, 1.0, 1.0}
        );

        // Paints every cell whose voxel centre lies inside the stamp body
        // (semi-axes f R S, f R, R) and whose direction does not face away
        // from the splat; returns how many changed id. A cell locked by a
        // running star keeps its owner against any other id.
        i32 Mark(
            const PaintLocalStamp& stamp,
            u8 paintId,
            u8 starGen,
            const PaintLockGens& locks,
            f32 coreFraction
        );
        i32 Mark(const PaintLocalStamp& stamp, u8 paintId, f32 coreFraction) {
            return Mark(stamp, paintId, 0, PaintLockGens{}, coreFraction);
        }
        void ClearPaint();

        PaintCoverage Coverage() const;
        PaintCoverage Coverage(PaintFaceDirection direction) const;

        bool IsBuilt() const noexcept { return surfaceCellCount > 0; }
        i32 SurfaceCellCount() const noexcept { return surfaceCellCount; }
        f32 CellSize() const noexcept { return cellSize; }
        const std::array<i32, 3>& Dims() const noexcept { return dims; }
        DVec3 Origin() const noexcept { return origin; }
        std::span<const u8> Ids() const noexcept { return ids; }

        void ForEachSurfaceCell(
            const std::function<void(
                Vec3 surfaceCenter,
                PaintFaceDirection direction,
                u8 paintId,
                u8 starGen,
                f32 area
            )>& visitor
        ) const;

    private:
        i32 voxelIndex(std::array<i32, 3> voxel) const noexcept;
        std::array<i32, 3> voxelOf(DVec3 position) const noexcept;
        DVec3 voxelCenter(std::array<i32, 3> voxel) const noexcept;
        void depositClipped(
            const std::vector<DVec3>& polygon,
            i32 axis,
            i32 direction
        );
    };
}
