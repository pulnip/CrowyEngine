#include "PaintSurface.hpp"

#include <cmath>
#include <utility>

namespace Crowy
{
    PaintSurface::PaintSurface(
        PaintStageObject object,
        const PaintMeshTriangles& mesh
    )
        : object(std::move(object)), mesh(&mesh) {
        Prepare();
    }

    void PaintSurface::Prepare() {
        meshLocalBounds = boundsOf(*mesh);
        scale3D = absolute(object.transform.scale);
        enabledDirections = resolveEnabledDirections();

        // the grid lives in the scaled-local frame: positions carry the
        // scale, so a cell stays a world-sized cube on every axis
        std::vector<Vec3> scaled;
        scaled.reserve(mesh->positions.size());
        const auto s = toVec3(scale3D);
        for(const auto& p: mesh->positions)
            scaled.push_back(p * s);
        cells.Build(
            ScaledBounds(),
            PaintScoreCellSize,
            1.0f,
            scaled,
            mesh->normals,
            mesh->indices,
            enabledDirections,
            scale3D
        );

        const auto texelCm = object.texelCmOverride > 0.0f
                                 ? object.texelCmOverride
                                 : PaintTexelSizeCm;
        layout = PaintIslandLayout::Build(
            meshLocalBounds,
            scale3D,
            enabledDirections,
            texelCm,
            PaintIslandPad,
            PaintMinRenderTargetSize,
            PaintMaxRenderTargetSize
        );
        ++layoutVersion;
    }

    void PaintSurface::SetDirections(u8 flags, bool floorFollowsWorldUp) {
        object.directions = flags;
        object.floorFollowsWorldUp = floorFollowsWorldUp;
        Prepare();
    }

    bool PaintSurface::IsWorldNormalPersistent(DVec3 worldNormal) const {
        // a hit normal is a geometric normal, which the inverse transpose
        // maps: undo the rotation, then multiply by the scale it divided out
        const auto localNormal =
            object.transform.InverseTransformVectorNoScale(worldNormal) *
            scale3D;
        const auto direction = classifyPaintFaceDirection(localNormal);

        return (enabledDirections & paintDirectionBit(direction)) != 0;
    }

    PaintLocalStamp PaintSurface::ComputeLocalStamp(
        const PaintSplat& splat
    ) const {
        const auto& t = object.transform;
        const auto axisV = cross(splat.normal, splat.axisU);

        return PaintLocalStamp{
            .center = t.InverseTransformPositionNoScale(splat.location),
            .axisU = t.InverseTransformVectorNoScale(splat.axisU),
            .axisV = t.InverseTransformVectorNoScale(axisV),
            .normal = t.InverseTransformVectorNoScale(splat.normal),
            .radius = splat.radius,
            .stretch = splat.stretch
        };
    }

    void PaintSurface::BuildStampRects(
        const PaintLocalStamp& stamp,
        std::vector<IntRect>& rects
    ) const {
        rects.clear();
        const auto bounds = ScaledBounds();
        const auto boundsSize = bounds.Size();
        // the brush keeps an exact edge distance this far outside the stamp
        const auto margin =
            static_cast<f64>(PaintDistanceRange + 1.0f) * layout.texelCm;

        DVec3 low;
        DVec3 high;
        for(usize a = 0; a < 3; ++a) {
            const auto extent =
                stamp.radius *
                    (stamp.stretch * std::abs(stamp.axisU[a]) +
                     std::abs(stamp.axisV[a]) + std::abs(stamp.normal[a])) +
                margin;
            const auto inv = boundsSize[a] > 1e-8 ? 1.0 / boundsSize[a] : 0.0;
            low[a] = (stamp.center[a] - extent - bounds.min[a]) * inv;
            high[a] = (stamp.center[a] + extent - bounds.min[a]) * inv;
        }

        for(const auto& island: layout.islands) {
            const auto from = island.ProjectNormalized(low);
            const auto to = island.ProjectNormalized(high);
            IntRect rect{
                {static_cast<i32>(std::floor(from.x)),
                 static_cast<i32>(std::floor(from.y))},
                {static_cast<i32>(std::ceil(to.x)),
                 static_cast<i32>(std::ceil(to.y))}
            };
            rect.Clip(island.rect);
            if(rect.Area() > 0)
                rects.push_back(rect);
        }
    }

    void PaintSurface::MarkScore(
        const PaintSplat& splat,
        const PaintLocalStamp& stamp
    ) {
        // a score-only splat stands for a mark a few cells wide at most; at
        // half its radius it would fall between cell centres, so it claims
        // all of it
        const auto coreFraction =
            splat.scoreOnly ? 1.0f : PaintCellStampFraction;
        cells.Mark(
            stamp,
            splat.paintId,
            splat.starGen,
            splat.lockGens,
            coreFraction
        );
    }

    PaintAtlasBakeInput PaintSurface::BakeInput() const {
        return PaintAtlasBakeInput{
            .positions = mesh->positions,
            .normals = mesh->normals,
            .indices = mesh->indices,
            .localBounds = meshLocalBounds,
            .layout = layout
        };
    }

    Box3d PaintSurface::ScaledBounds() const noexcept {
        Box3d bounds;
        bounds.Add(meshLocalBounds.min * scale3D);
        bounds.Add(meshLocalBounds.max * scale3D);

        return bounds;
    }

    u8 PaintSurface::resolveEnabledDirections() const {
        u8 mask = object.directions;
        if(!object.floorFollowsWorldUp)
            return mask;

        // the local direction facing the sky most is the floor players stand
        // on, however the actor was rolled; a sliver of a footprint, as a
        // wall's top edge, is not worth a buffer
        u8 best = 0;
        auto bestDot = -2.0;
        for(u8 d = 0; d < PaintFaceDirectionCount; ++d) {
            const auto worldAxis = object.transform.rotation.Rotate(
                paintFaceDirectionVector(paintFaceDirectionAt(d))
            );
            const auto up = dot(worldAxis, DVec3{0.0, 0.0, 1.0});
            if(up > bestDot) {
                bestDot = up;
                best = d;
            }
        }
        const auto size = meshLocalBounds.Size() * scale3D;
        const auto axis = best / 2;
        const auto footprint = size[(axis + 1) % 3] * size[(axis + 2) % 3];
        if(footprint >= PaintAutoUpMinIslandArea)
            mask |= paintDirectionBit(paintFaceDirectionAt(best));

        return mask;
    }
}
