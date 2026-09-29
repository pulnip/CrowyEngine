#include <array>
#include <cmath>

#include <gtest/gtest.h>

#include "FakeDevice.hpp"
#include "LinearAlgebra.hpp"
#include "RenderLight.hpp"
#include "RenderScene.hpp"
#include "SceneRenderer.hpp"

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

        const LightData& PackOne(const LightSnapshot& light) {
            Add(light);
            renderer.BeginFrame(scene);
            EXPECT_EQ(renderer.LightRows().size(), 1u);

            return renderer.LightRows().front();
        }
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
