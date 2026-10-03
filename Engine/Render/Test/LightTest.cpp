#include <array>
#include <cmath>

#include <gtest/gtest.h>

#include "FakeDevice.hpp"
#include "LinearAlgebra.hpp"
#include "RenderLight.hpp"
#include "RenderScene.hpp"
#include "SceneRenderer.hpp"
#include "ShadowFit.hpp"

using namespace Crowy;

namespace
{
    // a scene of lights alone, packed by one BeginFrame
    class Fixture {
    public:
        FakeDevice device;
        RenderScene scene;
        SceneRenderer renderer{device, SceneRendererDesc{.viewCount = 2}};

        LightHandle Add(const LightSnapshot& light) {
            return scene.Lights().Add(light);
        }

        // a primitive inside clip space for an identity view
        void AddPrimitive(
            AABB3D bounds,
            PrimitiveFlags flags =
                combine(PrimitiveFlags::Visible, PrimitiveFlags::CastShadow)
        ) {
            const auto material = scene.Materials().Add(MaterialResource{});
            const auto mesh = scene.Meshes().Add(
                MeshResource{
                    .subMeshes = {SubMesh{.geometry = {.indexCount = 3}}},
                    .materials = {material}
                }
            );
            scene.Primitives().Add(
                PrimitiveSnapshot{
                    .worldBounds = bounds,
                    .mesh = mesh,
                    .flags = flags
                }
            );
        }

        const LightData& PackOne(const LightSnapshot& light) {
            Add(light);
            renderer.BeginFrame(scene);
            EXPECT_EQ(renderer.LightRows().size(), 1u);

            return renderer.LightRows().front();
        }
    };

    constexpr u32 MapSize = 64;
    // corners exact in binary, so the union BeginFrame rebuilds from them
    // is this box bit for bit
    constexpr AABB3D Box{
        .center = {0.0f, 0.0f, 0.5f},
        .halfScale = {0.25f, 0.25f, 0.25f}
    };

    const LightSnapshot CastingSun{
        .castShadow = true,
        .direction = {0.25f, -0.866f, 0.433f}
    };

    // the cone term the shader takes, before its saturate
    f32 Cone(const LightData& row, f32 cosAngle) {
        return cosAngle * row.coneScale + row.coneOffset;
    }
}

TEST(Light, ADirectionalRowIsNormalizedAndScaled) {
    Fixture f;
    const auto& row = f.PackOne(
        LightSnapshot{
            .color = {0.25f, 0.5f, 1.0f},
            .intensity = 4.0f,
            .direction = {0.0f, -2.0f, 0.0f}
        }
    );

    EXPECT_EQ(row.kind, LightKind::Directional);
    EXPECT_EQ(row.direction, (Vec3{0.0f, -1.0f, 0.0f}));
    EXPECT_EQ(row.color, (Vec3{1.0f, 2.0f, 4.0f}));
    EXPECT_EQ(row.invRange, 0.0f);
    EXPECT_EQ(row.coneScale, 0.0f);
    EXPECT_EQ(row.coneOffset, 1.0f);
    EXPECT_EQ(row.shadowIndex, NoShadow);
    EXPECT_EQ(row.worldToShadow, unitMat());
}

TEST(Light, APointRowCarriesInvRangeAndTheNeutralCone) {
    Fixture f;
    const auto& row = f.PackOne(
        LightSnapshot{
            .kind = LightKind::Point,
            .position = {1.0f, 2.0f, 3.0f},
            .range = 4.0f
        }
    );

    EXPECT_EQ(row.kind, LightKind::Point);
    EXPECT_EQ(row.position, (Vec3{1.0f, 2.0f, 3.0f}));
    EXPECT_FLOAT_EQ(row.invRange, 0.25f);
    EXPECT_EQ(row.coneScale, 0.0f);
    EXPECT_EQ(row.coneOffset, 1.0f);
    EXPECT_EQ(row.shadowIndex, NoShadow);
}

TEST(Light, ASpotRowCarriesTheGltfCone) {
    constexpr f32 Inner = 0.2f;
    constexpr f32 Outer = 0.5f;

    Fixture f;
    const auto& row = f.PackOne(
        LightSnapshot{
            .kind = LightKind::Spot,
            .range = 5.0f,
            .innerConeAngle = Inner,
            .outerConeAngle = Outer
        }
    );

    EXPECT_EQ(row.kind, LightKind::Spot);
    EXPECT_FLOAT_EQ(row.invRange, 0.2f);
    EXPECT_NEAR(Cone(row, std::cos(Outer)), 0.0f, 1e-5f);
    EXPECT_NEAR(Cone(row, std::cos(Inner)), 1.0f, 1e-5f);
    // straight down the axis the saturate keeps it at 1
    EXPECT_GT(Cone(row, 1.0f), 1.0f);
}

TEST(Light, DisabledRowsAreSkipped) {
    Fixture f;
    const std::array handles{
        f.Add(LightSnapshot{.intensity = 1.0f}),
        f.Add(LightSnapshot{.enabled = false, .intensity = 2.0f}),
        f.Add(LightSnapshot{.intensity = 3.0f})
    };

    f.renderer.BeginFrame(f.scene);
    f.renderer.Upload();

    // table order, the disabled row gone
    const auto rows = f.renderer.LightRows();
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_EQ(rows[0].color.x, 1.0f);
    EXPECT_EQ(rows[1].color.x, 3.0f);

    // the push names the uploaded rows
    constexpr auto Stride = static_cast<u32>(sizeof(LightData));
    const auto push = f.renderer.FramePush();
    EXPECT_EQ(push.lights, FakeBuffer::ReadableID);
    EXPECT_EQ(push.lightCount, 2u);
    EXPECT_EQ(
        f.device.transient.Read<LightData>((push.lightBase + 1) * Stride)
            .color.x,
        3.0f
    );

    // every row disabled: nothing uploaded, nothing named
    for(const auto handle: handles)
        f.scene.Lights().GetRef(handle).enabled = false;
    f.renderer.BeginFrame(f.scene);
    const auto allocations = f.device.transientAllocations;
    f.renderer.Upload();

    EXPECT_TRUE(f.renderer.LightRows().empty());
    // the views' slice alone
    EXPECT_EQ(f.device.transientAllocations, allocations + 1);
    EXPECT_EQ(f.renderer.FramePush().lights, 0u);
    EXPECT_EQ(f.renderer.FramePush().lightBase, 0u);
    EXPECT_EQ(f.renderer.FramePush().lightCount, 0u);
}

TEST(Light, TheEnvironmentReachesEveryView) {
    constexpr Vec3 Sky{0.1f, 0.2f, 0.3f};
    constexpr Vec3 Ground{0.4f, 0.5f, 0.6f};

    Fixture f;
    f.scene.Environment() =
        EnvironmentSnapshot{.skyAmbient = Sky, .groundAmbient = Ground};

    f.renderer.BeginFrame(f.scene);

    for(u32 i = 0; i < f.renderer.ViewCount(); ++i) {
        EXPECT_EQ(
            static_cast<Vec3>(f.renderer.View(i).skyAmbient),
            Sky
        ) << i;
        EXPECT_EQ(
            static_cast<Vec3>(f.renderer.View(i).groundAmbient),
            Ground
        ) << i;
    }
}

TEST(Light, AZeroDirectionFallsBackToDown) {
    Fixture f;
    const auto& row = f.PackOne(LightSnapshot{.direction = zeros()});

    EXPECT_EQ(row.direction, -unitY());
}

TEST(Light, ANonPositiveRangeIsFlooredAtAMillimetre) {
    for(const auto range: {0.0f, -1.0f}) {
        Fixture f;
        const auto& row = f.PackOne(
            LightSnapshot{.kind = LightKind::Point, .range = range}
        );

        EXPECT_TRUE(std::isfinite(row.invRange)) << range;
        EXPECT_FLOAT_EQ(row.invRange, 1000.0f) << range;
    }
}

TEST(Light, OnlyTheFirstShadowCastingDirectionalGetsShadowZero) {
    Fixture f;
    f.AddPrimitive(Box);
    f.Add(LightSnapshot{.kind = LightKind::Point, .castShadow = true});
    f.Add(LightSnapshot{.castShadow = true, .intensity = 1.0f});
    f.Add(LightSnapshot{.castShadow = true, .intensity = 2.0f});
    f.Add(LightSnapshot{.intensity = 3.0f});

    f.renderer.BeginFrame(f.scene, MapSize);

    const auto rows = f.renderer.LightRows();
    ASSERT_EQ(rows.size(), 4u);
    EXPECT_EQ(rows[0].shadowIndex, NoShadow);
    EXPECT_EQ(rows[1].shadowIndex, 0u);
    EXPECT_EQ(rows[2].shadowIndex, NoShadow);
    EXPECT_EQ(rows[3].shadowIndex, NoShadow);
    EXPECT_EQ(rows[2].worldToShadow, unitMat());
}

TEST(Light, TheShadowedRowCarriesTheFit) {
    Fixture f;
    f.AddPrimitive(Box);
    // hidden, so outside the union
    f.AddPrimitive(
        AABB3D{.center = {50.0f, 0.0f, 0.0f}},
        PrimitiveFlags::CastShadow
    );
    auto sun = CastingSun;
    sun.shadowBias = 0.05f;
    sun.shadowNormalBias = 2.0f;
    f.Add(sun);

    f.renderer.BeginFrame(f.scene, MapSize);

    const auto& row = f.renderer.LightRows().front();
    const auto fit = fitDirectionalShadow(row.direction, Box, MapSize);
    EXPECT_EQ(row.shadowIndex, 0u);
    EXPECT_EQ(row.worldToShadow, fit.worldToShadow);
    // casters and receivers agree bit for bit
    EXPECT_EQ(
        f.renderer.View(SceneRenderer::ShadowView).viewProj,
        row.worldToShadow
    );
    EXPECT_EQ(row.shadowBias, 0.05f);
    EXPECT_FLOAT_EQ(row.shadowNormalBias, 2.0f * fit.texelSize);
    // the view culls against the fit, which holds the visible box
    EXPECT_EQ(f.renderer.Visible(SceneRenderer::ShadowView).primitiveCount, 1u);
}

// a far plane that casts nothing, as a sea, leaves the fit to the casters
TEST(Light, AVisibleNonCasterStaysOutsideTheFit) {
    Fixture f;
    f.AddPrimitive(Box);
    f.AddPrimitive(
        AABB3D{
            .center = {50.0f, 0.0f, 0.0f},
            .halfScale = {40.0f, 0.1f, 40.0f}
        },
        PrimitiveFlags::Visible
    );
    f.Add(CastingSun);

    f.renderer.BeginFrame(f.scene, MapSize);

    const auto& row = f.renderer.LightRows().front();
    const auto fit = fitDirectionalShadow(row.direction, Box, MapSize);
    EXPECT_EQ(row.shadowIndex, 0u);
    EXPECT_EQ(row.worldToShadow, fit.worldToShadow);
}

// With a map but nothing to fit, row 1 is identity and its set is empty
// without a cull: the box sits inside identity's clip space, so a cull
// would keep it
TEST(Light, AnUnfittedShadowViewIsEmptyWithoutACull) {
    {
        // a casting sun, and nothing Visible to fit
        Fixture f;
        f.AddPrimitive(Box, PrimitiveFlags::CastShadow);
        f.Add(CastingSun);
        f.renderer.View(SceneRenderer::ShadowView).viewProj =
            translateMat({3.0f, 0.0f, 0.0f});

        f.renderer.BeginFrame(f.scene, MapSize);

        EXPECT_EQ(f.renderer.LightRows().front().shadowIndex, NoShadow);
        EXPECT_EQ(
            f.renderer.View(SceneRenderer::ShadowView).viewProj,
            unitMat()
        );
        EXPECT_EQ(
            f.renderer.Visible(SceneRenderer::ShadowView).primitiveCount,
            0u
        );
    }
    {
        // something Visible, and no light that casts
        Fixture f;
        f.AddPrimitive(Box);
        f.Add(LightSnapshot{});

        f.renderer.BeginFrame(f.scene, MapSize);

        const auto& visible = f.renderer.Visible(SceneRenderer::ShadowView);
        EXPECT_EQ(visible.primitiveCount, 0u);
        EXPECT_TRUE(visible.draws.empty());
        // the main view still culls
        EXPECT_EQ(f.renderer.Visible(0).primitiveCount, 1u);
    }
}

// row 1 stays the caller's without a map
TEST(Light, WithoutAMapEveryRowGetsNoShadow) {
    const auto byHand = translateMat({3.0f, 0.0f, 0.0f});

    Fixture f;
    f.AddPrimitive(Box);
    f.Add(CastingSun);
    f.renderer.View(SceneRenderer::ShadowView).viewProj = byHand;

    f.renderer.BeginFrame(f.scene);

    const auto& row = f.renderer.LightRows().front();
    EXPECT_EQ(row.shadowIndex, NoShadow);
    EXPECT_EQ(row.worldToShadow, unitMat());
    EXPECT_EQ(row.shadowNormalBias, 0.0f);
    EXPECT_EQ(f.renderer.View(SceneRenderer::ShadowView).viewProj, byHand);
}
