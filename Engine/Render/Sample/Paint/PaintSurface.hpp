#pragma once

#include <vector>

#include "MintFrame.hpp"
#include "PaintAtlasBaker.hpp"
#include "PaintCellGrid.hpp"
#include "PaintIslandLayout.hpp"
#include "PaintMeshes.hpp"
#include "PaintStage.hpp"
#include "PaintTypes.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    // UPaintableComponent's CPU half: which directions keep paint, the atlas
    // layout one buffer is cut into, and every frame change a splat goes
    // through on its way to the brush and the score
    class PaintSurface {
    private:
        PaintStageObject object;
        const PaintMeshTriangles* mesh = nullptr;
        // the mesh's own bounds: what the position atlas is normalized to
        Box3d meshLocalBounds;
        // the absolute world scale, the factor between local and scaled-local
        DVec3 scale3D{1.0, 1.0, 1.0};
        u8 enabledDirections = 0;
        PaintIslandLayout layout;
        // bumped by every re-layout, so the GPU half knows to rebuild
        u64 layoutVersion = 0;
        PaintCellGrid cells;

    public:
        PaintSurface(PaintStageObject object, const PaintMeshTriangles& mesh);

        // the component's direction flags, changed at run time
        void SetDirections(u8 flags, bool floorFollowsWorldUp);

        // whether a splat landing with this world normal is kept and scored
        bool IsWorldNormalPersistent(DVec3 worldNormal) const;
        PaintLocalStamp ComputeLocalStamp(const PaintSplat& splat) const;
        // the atlas rectangles the stamp can touch, one per island it
        // reaches, the distance ramp's margin included
        void BuildStampRects(
            const PaintLocalStamp& stamp,
            std::vector<IntRect>& rects
        ) const;
        PaintAtlasBakeInput BakeInput() const;
        // the score half of a splat, which needs only the grid: the same
        // local stamp the brush draws, so the two differ only by the stamp's
        // satellites and the cell size
        void MarkScore(const PaintSplat& splat, const PaintLocalStamp& stamp);
        void ClearScore() { cells.ClearPaint(); }

        const PaintStageObject& Object() const noexcept { return object; }
        const PaintMeshTriangles& Mesh() const noexcept { return *mesh; }
        const Box3d& LocalBounds() const noexcept { return meshLocalBounds; }
        Box3d ScaledBounds() const noexcept;
        DVec3 Scale3D() const noexcept { return scale3D; }
        u8 EnabledDirections() const noexcept { return enabledDirections; }
        const PaintIslandLayout& Layout() const noexcept { return layout; }
        u64 LayoutVersion() const noexcept { return layoutVersion; }
        const PaintCellGrid& Cells() const noexcept { return cells; }

    private:
        // PrepareSurface and the layout BeginPlay builds
        void prepare();
        u8 resolveEnabledDirections() const;
    };
}
