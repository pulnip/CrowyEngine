#include <algorithm>
#include <array>
#include <numbers>

#include <gtest/gtest.h>

#include "BacklotContent.hpp"
#include "EnumUtil.hpp"
#include "StageScene.hpp"

using namespace Crowy;

namespace
{
    // Backlot's inventory per lighting key
    struct KeyInventory {
        CStr key = "";
        usize instances = 0;
        u64 triangles = 0;
        usize lights = 0;
    };

    const LoadedStage& backlotStage() {
        static const auto stage = loadStage(backlotRoot());

        return stage;
    }
}

// the rows each key shows, without a device: empty handles stand in for the
// pool's allocations and the uploaded images
TEST(BacklotScene, EachKeyShowsItsInventory) {
    constexpr std::array Inventory{
        KeyInventory{.key = "day", .instances = 1328, .triangles = 466462, .lights = 0},
        KeyInventory{.key = "evening", .instances = 1439, .triangles = 469102, .lights = 31},
        KeyInventory{.key = "sunset", .instances = 1439, .triangles = 469102, .lights = 31},
        KeyInventory{.key = "night", .instances = 1488, .triangles = 470566, .lights = 31},
    };

    const auto& stage = backlotStage();
    const auto& document = stage.document;
    StageGeometry geometry;
    for(const auto& model: stage.models)
        geometry.models.emplace_back(model.slots.size());
    StageTextureHandles textures;
    for(const auto& material: document.materials)
        textures.emplace(material.texture, TextureHandle{});
    RenderScene scene;
    const auto bindings = populateStage(scene, stage, geometry, textures);
    EXPECT_EQ(scene.Materials().Count(), countStageMaterials(stage));

    const auto visible = [&](PrimitiveHandle primitive) {
        return hasFlag(scene.Primitives().GetRef(primitive).flags, PrimitiveFlags::Visible);
    };
    for(const auto& expected: Inventory) {
        applyStageKey(scene, bindings, document, stageKeyIndex(document, expected.key));

        usize instances = 0;
        u64 triangles = 0;
        for(usize i = 0; i < bindings.instances.size(); ++i) {
            if(!visible(bindings.instances[i]))
                continue;
            ++instances;
            const auto model = std::ranges::find(document.models, document.instances[i].model, &StageModel::id);
            for(const auto& slot: stage.models[static_cast<usize>(model - document.models.begin())].slots)
                triangles += slot.mesh.indices.size() / 3;
        }
        const auto quads = std::ranges::count_if(bindings.quads, visible);
        const auto lights = std::ranges::count_if(bindings.lights, [&](LightHandle light) {
            return scene.Lights().GetRef(light).enabled;
        });

        EXPECT_EQ(instances, expected.instances) << expected.key;
        EXPECT_EQ(triangles, expected.triangles) << expected.key;
        EXPECT_EQ(quads, 161) << expected.key;
        EXPECT_EQ(static_cast<usize>(lights), expected.lights) << expected.key;
    }
}

TEST(BacklotScene, CutsTakeTheEnginesSignsAndFittedNearPlanes) {
    const auto cuts = makeStageCuts(backlotStage());
    const auto named = [&](StrView name) -> const StageCut& {
        return *std::ranges::find(cuts, name, &StageCut::name);
    };
    ASSERT_EQ(cuts.size(), 8u);

    const auto& establishing = named("establishing");
    EXPECT_NEAR(establishing.yaw, std::numbers::pi_v<f32> / 4, 1e-4f);
    EXPECT_NEAR(establishing.pitch, 0.2793f, 1e-4f);
    EXPECT_NEAR(establishing.fovY, 0.6981f, 1e-4f);
    EXPECT_FLOAT_EQ(establishing.nearZ, 1.0f);
    // half the 1.42 m between the street camera and the nearest box
    EXPECT_NEAR(named("street").nearZ, 0.71f, 1e-3f);

    const auto& plan = named("plan");
    EXPECT_TRUE(plan.orthographic);
    EXPECT_FLOAT_EQ(plan.orthoHalfHeight, 52.0f);
    EXPECT_NEAR(plan.pitch, std::numbers::pi_v<f32> / 2, 1e-5f);
}
