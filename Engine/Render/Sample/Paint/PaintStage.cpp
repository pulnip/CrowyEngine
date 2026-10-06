#include "PaintStage.hpp"

#include <format>

namespace Crowy
{
    namespace
    {
        constexpr f64 CubeSize = 100.0;

        PaintStageObject box(
            Str name,
            DVec3 center,
            DVec3 size,
            u8 directions = paintDirectionBit(PaintFaceDirection::Up)
        ) {
            return PaintStageObject{
                .name = std::move(name),
                .mesh = PaintMeshKind::Cube,
                .transform =
                    MintTransform{
                        .translation = center,
                        .scale = size * (1.0 / CubeSize)
                    },
                .directions = directions
            };
        }
    }

    const PaintMeshTriangles& PaintStageMeshes::Get(
        PaintMeshKind kind
    ) const noexcept {
        using enum PaintMeshKind;

        switch(kind) {
        case Cube:
            return cube;
        case Wedge:
            return wedge;
        case Sphere:
            return sphere;
        }

        return cube;
    }

    PaintStageMeshes makePaintStageMeshes() {
        return PaintStageMeshes{
            .cube = makePaintCube(CubeSize),
            .wedge = makePaintWedge({500.0, 300.0, 150.0}),
            .sphere = makePaintSphere(120.0, 48, 24)
        };
    }

    std::vector<PaintStageObject> makePaintStageObjects() {
        using enum PaintFaceDirection;
        std::vector<PaintStageObject> objects;

        // the floor outgrows the largest atlas, so its texel coarsens
        objects.push_back(
            box("floor", {0.0, 0.0, -10.0}, {2000.0, 2000.0, 20.0})
        );
        // Up only to start; the topic turns Front and Right on
        objects.push_back(
            box("crate", {-500.0, -300.0, 150.0}, {300.0, 300.0, 300.0})
        );
        // its stage-facing side is -Y
        objects.push_back(
            box("wall",
                {0.0, 650.0, 150.0},
                {800.0, 60.0, 300.0},
                paintDirectionBit(Up) | paintDirectionBit(Left))
        );
        objects.push_back(
            PaintStageObject{
                .name = "ramp",
                .mesh = PaintMeshKind::Wedge,
                .transform = MintTransform{.translation = {500.0, -450.0, 0.0}}
            }
        );
        // every direction, so a curve changes island as it turns
        objects.push_back(
            PaintStageObject{
                .name = "sphere",
                .mesh = PaintMeshKind::Sphere,
                .transform =
                    MintTransform{.translation = {500.0, 350.0, 120.0}},
                .directions = PaintAllDirectionsMask
            }
        );
        // the shipped stage floor's texel, for what game paint really looks
        // like
        auto pad = box("pad", {0.0, -1500.0, -5.0}, {800.0, 800.0, 10.0});
        pad.texelCmOverride = 4.24f;
        objects.push_back(pad);

        for(usize speed = 0; speed < PaintGridSide; ++speed) {
            for(usize theta = 0; theta < PaintGridSide; ++theta) {
                const auto offset = 0.5 * static_cast<f64>(PaintGridSide - 1);
                auto tile =
                    box(std::format("tile{}{}", speed, theta),
                        {(offset - static_cast<f64>(speed)) * PaintGridPitch,
                         (static_cast<f64>(theta) - offset) * PaintGridPitch,
                         -10.0},
                        {PaintGridTileSize, PaintGridTileSize, 20.0});
                tile.stage = PaintStageKind::Grid;
                objects.push_back(tile);
            }
        }

        return objects;
    }

    usize paintGridTileIndex(usize theta, usize speed) {
        // the block stage's six come first
        constexpr usize BlockCount = 6;

        return BlockCount + speed * PaintGridSide + theta;
    }
}
