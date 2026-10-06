#include "PaintCellGrid.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace Crowy
{
    namespace
    {
        // Sutherland-Hodgman against one axis-aligned plane; the polygons
        // stay convex, so this is exact
        void clipToHalfSpace(
            const std::vector<DVec3>& in,
            std::vector<DVec3>& out,
            i32 axis,
            f64 plane,
            bool keepAbove
        ) {
            out.clear();
            const auto count = in.size();
            for(usize i = 0; i < count; ++i) {
                const auto& p = in[i];
                const auto& q = in[(i + 1) % count];
                const auto distP =
                    keepAbove ? p[axis] - plane : plane - p[axis];
                const auto distQ =
                    keepAbove ? q[axis] - plane : plane - q[axis];
                if(distP >= 0.0)
                    out.push_back(p);
                if((distP >= 0.0) != (distQ >= 0.0))
                    out.push_back(p + (q - p) * (distP / (distP - distQ)));
            }
        }
    }

    void PaintCellGrid::Build(
        const Box3d& bounds,
        f32 cell,
        f32 areaScale,
        std::span<const Vec3> positions,
        std::span<const Vec3> normals,
        std::span<const u32> indices,
        u8 enabledDirections,
        DVec3 normalClassifyScale
    ) {
        // a huge mesh with a tiny cell doubles its cell until the grid fits
        constexpr i64 MaxVoxels = i64{1} << 20;

        origin = bounds.min;
        cellSize = std::max(cell, KindaSmallNumber);
        const auto size = bounds.Size();
        const auto computeDims = [&size](f32 c) {
            return std::array<i32, 3>{
                std::max(1, static_cast<i32>(std::ceil(size.x / c))),
                std::max(1, static_cast<i32>(std::ceil(size.y / c))),
                std::max(1, static_cast<i32>(std::ceil(size.z / c)))
            };
        };
        dims = computeDims(cellSize);
        while(static_cast<i64>(dims[0]) * dims[1] * dims[2] > MaxVoxels) {
            cellSize *= 2.0f;
            dims = computeDims(cellSize);
        }

        const auto cellCount = static_cast<usize>(dims[0]) * dims[1] * dims[2] *
                               PaintFaceDirectionCount;
        ids.assign(cellCount, PaintIdNone);
        starGens.assign(cellCount, 0);
        areas.assign(cellCount, 0.0f);
        surfaceCenters.assign(cellCount, Vec3{});
        std::memset(totals, 0, sizeof(totals));
        totalArea = 0.0f;
        surfaceCellCount = 0;

        const bool hasNormals = normals.size() == positions.size();
        const auto vertexCount = static_cast<u32>(positions.size());
        for(usize first = 0; first + 2 < indices.size(); first += 3) {
            const auto i0 = indices[first];
            const auto i1 = indices[first + 1];
            const auto i2 = indices[first + 2];
            if(i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount)
                continue;

            const auto a = toDVec3(positions[i0]);
            const auto b = toDVec3(positions[i1]);
            const auto c = toDVec3(positions[i2]);
            const auto crossed = cross(b - a, c - a);
            const auto doubleArea = Crowy::size(crossed);
            if(doubleArea <= SmallNumber)
                continue;

            // the sign depends on the winding; the vertex normals settle it
            auto faceNormal = crossed / doubleArea;
            if(hasNormals) {
                const auto vertexNormal = toDVec3(normals[i0]) +
                                          toDVec3(normals[i1]) +
                                          toDVec3(normals[i2]);
                if(dot(faceNormal, vertexNormal) < 0.0)
                    faceNormal = -faceNormal;
            }
            const auto face =
                classifyPaintFaceDirection(faceNormal * normalClassifyScale);
            if(!(enabledDirections & paintDirectionBit(face)))
                continue;

            // exact rather than sampled: every voxel the triangle crosses gets
            // its piece's true area
            depositClipped({a, b, c}, 0, static_cast<i32>(face), areaScale);
        }

        for(usize i = 0; i < cellCount; ++i) {
            if(areas[i] > 0.0f)
                surfaceCenters[i] = surfaceCenters[i] / areas[i];
        }
    }

    void PaintCellGrid::depositClipped(
        const ClipPolygon& polygon,
        i32 axis,
        i32 direction,
        f32 areaScale
    ) {
        if(polygon.size() < 3)
            return;

        if(axis == 3) {
            // inside one voxel: fan-triangulate for the area and centroid
            DVec3 crossSum;
            DVec3 centroidSum;
            for(usize i = 1; i + 1 < polygon.size(); ++i) {
                const auto c =
                    cross(polygon[i] - polygon[0], polygon[i + 1] - polygon[0]);
                crossSum = crossSum + c;
                centroidSum =
                    centroidSum + (polygon[0] + polygon[i] + polygon[i + 1]) *
                                      (Crowy::size(c) / 3.0);
            }
            const auto doubleArea = Crowy::size(crossSum);
            if(doubleArea <= SmallNumber)
                return;
            const auto area = static_cast<f32>(0.5 * doubleArea) * areaScale;
            const auto centroid = centroidSum / doubleArea;

            const auto cell = static_cast<usize>(
                voxelIndex(voxelOf(centroid)) * PaintFaceDirectionCount +
                direction
            );
            if(areas[cell] <= 0.0f)
                ++surfaceCellCount;
            areas[cell] += area;
            surfaceCenters[cell] =
                surfaceCenters[cell] + toVec3(centroid) * area;
            totals[direction][PaintIdNone] += area;
            totalArea += area;
            return;
        }

        auto lo = std::numeric_limits<f64>::max();
        auto hi = std::numeric_limits<f64>::lowest();
        for(const auto& p: polygon) {
            lo = std::min(lo, p[axis]);
            hi = std::max(hi, p[axis]);
        }
        const auto last = dims[axis] - 1;
        const auto firstVoxel = std::clamp(
            static_cast<i32>(std::floor((lo - origin[axis]) / cellSize)),
            0,
            last
        );
        const auto lastVoxel = std::clamp(
            static_cast<i32>(std::floor((hi - origin[axis]) / cellSize)),
            0,
            last
        );

        ClipPolygon above;
        ClipPolygon slab;
        for(auto voxel = firstVoxel; voxel <= lastVoxel; ++voxel) {
            // int times float, as Unreal's Voxel * CellSize rounds it
            const auto low =
                origin[axis] +
                static_cast<f64>(static_cast<f32>(voxel) * cellSize);
            // the outermost voxels keep whatever pokes past the bounds, so an
            // edge triangle never loses area to clamping
            clipToHalfSpace(polygon, above, axis, voxel == 0 ? lo : low, true);
            clipToHalfSpace(
                above,
                slab,
                axis,
                voxel == last ? hi : low + cellSize,
                false
            );
            depositClipped(slab, axis + 1, direction, areaScale);
        }
    }

    i32 PaintCellGrid::Mark(
        const PaintLocalStamp& stamp,
        u8 paintId,
        u8 starGen,
        const PaintLockGens& locks,
        f32 coreFraction
    ) {
        if(!IsBuilt() || paintId >= PaintIdCount)
            return 0;

        // the buffer byte's clamp and "unpainted carries no generation"
        const auto gen = decodePaintStarGen(encodePaintTexel(paintId, starGen));

        // f of the radius across the surface, but the full radius along the
        // normal, as the stamp shader shrinks its disc with sqrt(1 - n^2)
        const auto surfaceRadius =
            std::max(stamp.radius * coreFraction, KindaSmallNumber);
        const auto stretchedRadius =
            surfaceRadius * std::max(stamp.stretch, 1.0f);
        const auto depthRadius = std::max(stamp.radius, KindaSmallNumber);
        const auto reach =
            std::max({surfaceRadius, stretchedRadius, depthRadius});

        const auto low = voxelOf(stamp.center - DVec3{reach, reach, reach});
        const auto high = voxelOf(stamp.center + DVec3{reach, reach, reach});

        i32 changed = 0;
        for(auto z = low[2]; z <= high[2]; ++z) {
            for(auto y = low[1]; y <= high[1]; ++y) {
                for(auto x = low[0]; x <= high[0]; ++x) {
                    const std::array<i32, 3> voxel{x, y, z};
                    const auto d = voxelCenter(voxel) - stamp.center;
                    const auto u = dot(d, stamp.axisU) / stretchedRadius;
                    const auto v = dot(d, stamp.axisV) / surfaceRadius;
                    const auto n = dot(d, stamp.normal) / depthRadius;
                    if(u * u + v * v + n * n > 1.0)
                        continue;

                    const auto base = static_cast<usize>(voxelIndex(voxel)) *
                                      PaintFaceDirectionCount;
                    for(u8 direction = 0; direction < PaintFaceDirectionCount;
                        ++direction) {
                        const auto cell = base + direction;
                        if(areas[cell] <= 0.0f)
                            continue;
                        // the stamp reaches through thin geometry and the
                        // picture paints the far side; ownership does not
                        const auto faceNormal = paintFaceDirectionVector(
                            paintFaceDirectionAt(direction)
                        );
                        if(dot(faceNormal, stamp.normal) < -KindaSmallNumber)
                            continue;

                        // a running star's trail: no other id takes it, and
                        // its own id leaves the locked generation alone
                        const auto previous = ids[cell];
                        const bool sameId = previous == paintId;
                        const bool locked =
                            locks.Locks(previous, starGens[cell]);
                        if(locked && !sameId)
                            continue;
                        if(sameId) {
                            if(!locked)
                                starGens[cell] = gen;
                            continue;
                        }

                        totals[direction][previous] -= areas[cell];
                        totals[direction][paintId] += areas[cell];
                        ids[cell] = paintId;
                        starGens[cell] = gen;
                        ++changed;
                    }
                }
            }
        }

        return changed;
    }

    void PaintCellGrid::ClearPaint() {
        std::memset(totals, 0, sizeof(totals));
        for(usize i = 0; i < ids.size(); ++i) {
            ids[i] = PaintIdNone;
            starGens[i] = 0;
            totals[i % PaintFaceDirectionCount][PaintIdNone] += areas[i];
        }
    }

    PaintCoverage PaintCellGrid::Coverage() const {
        PaintCoverage coverage;
        for(usize d = 0; d < PaintFaceDirectionCount; ++d) {
            for(usize id = 0; id < PaintIdCount; ++id)
                coverage.areaByPaintId[id] += totals[d][id];
        }
        coverage.totalArea = totalArea;

        return coverage;
    }

    PaintCoverage PaintCellGrid::Coverage(PaintFaceDirection direction) const {
        PaintCoverage coverage;
        const auto row = static_cast<usize>(direction);
        for(usize id = 0; id < PaintIdCount; ++id) {
            coverage.areaByPaintId[id] = totals[row][id];
            coverage.totalArea += totals[row][id];
        }

        return coverage;
    }

    void PaintCellGrid::ForEachSurfaceCell(
        const std::function<void(Vec3, PaintFaceDirection, u8, u8, f32)>&
            visitor
    ) const {
        for(usize i = 0; i < areas.size(); ++i) {
            if(areas[i] > 0.0f) {
                visitor(
                    surfaceCenters[i],
                    paintFaceDirectionAt(
                        static_cast<u8>(i % PaintFaceDirectionCount)
                    ),
                    ids[i],
                    starGens[i],
                    areas[i]
                );
            }
        }
    }

    i32 PaintCellGrid::voxelIndex(std::array<i32, 3> voxel) const noexcept {
        return (voxel[2] * dims[1] + voxel[1]) * dims[0] + voxel[0];
    }

    std::array<i32, 3> PaintCellGrid::voxelOf(DVec3 position) const noexcept {
        const auto scaled = (position - origin) / static_cast<f64>(cellSize);

        return {
            std::clamp(static_cast<i32>(std::floor(scaled.x)), 0, dims[0] - 1),
            std::clamp(static_cast<i32>(std::floor(scaled.y)), 0, dims[1] - 1),
            std::clamp(static_cast<i32>(std::floor(scaled.z)), 0, dims[2] - 1)
        };
    }

    DVec3 PaintCellGrid::voxelCenter(std::array<i32, 3> voxel) const noexcept {
        return origin + DVec3{voxel[0] + 0.5, voxel[1] + 0.5, voxel[2] + 0.5} *
                            static_cast<f64>(cellSize);
    }
}
