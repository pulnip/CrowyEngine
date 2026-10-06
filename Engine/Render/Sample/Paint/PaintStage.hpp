#pragma once

#include <vector>

#include "MintFrame.hpp"
#include "PaintMeshes.hpp"
#include "PaintTypes.hpp"
#include "Primitives.hpp"

// PaintLab's two stages in Mint cm: the blocks every topic is shown on, and
// the 4x4 grid of floors the splat variables are compared on
namespace Crowy
{
    enum class PaintMeshKind : u8 {
        Cube,
        Wedge,
        Sphere,
    };

    inline constexpr usize PaintMeshKindCount = 3;

    enum class PaintStageKind : u8 {
        Block,
        Grid,
    };

    // the grid's axes: incidence across, impact speed down
    inline constexpr usize PaintGridSide = 4;
    inline constexpr f32
        PaintGridThetaDeg[PaintGridSide]{0.0f, 50.0f, 60.0f, 70.0f};
    inline constexpr f32
        PaintGridSpeed[PaintGridSide]{500.0f, 1500.0f, 3000.0f, 4800.0f};
    inline constexpr f64 PaintGridTileSize = 400.0;
    inline constexpr f64 PaintGridPitch = 600.0;

    struct PaintStageMeshes {
        PaintMeshTriangles cube;
        PaintMeshTriangles wedge;
        PaintMeshTriangles sphere;

        const PaintMeshTriangles& Get(PaintMeshKind kind) const noexcept;
    };

    // an actor with a paintable component: its mesh, where it stands, and
    // the component's direction flags
    struct PaintStageObject {
        Str name;
        PaintStageKind stage = PaintStageKind::Block;
        PaintMeshKind mesh = PaintMeshKind::Cube;
        MintTransform transform;
        u8 directions = paintDirectionBit(PaintFaceDirection::Up);
        bool floorFollowsWorldUp = true;
        // 0 keeps the project's 0.5 cm request
        f32 texelCmOverride = 0.0f;
    };

    PaintStageMeshes makePaintStageMeshes();
    std::vector<PaintStageObject> makePaintStageObjects();

    // the grid tile at column `theta` and row `speed`, both 0-based
    usize paintGridTileIndex(usize theta, usize speed);
}
