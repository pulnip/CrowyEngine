#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <sstream>

#include <gtest/gtest.h>

#include "BacklotContent.hpp"
#include "EnumUtil.hpp"
#include "StageScene.hpp"

using namespace Crowy;

namespace
{
    const LoadedStage& backlotStage() {
        static const auto stage = loadStage(backlotRoot());

        return stage;
    }

    // Backlot's scene file with `from` replaced by `to` once, in a temporary file
    std::filesystem::path editedScene(StrView name, StrView from, StrView to) {
        std::ifstream in(backlotRoot() / StageScenePath, std::ios::binary);
        std::stringstream text;
        text << in.rdbuf();
        auto scene = text.str();
        const auto at = scene.find(from);
        EXPECT_NE(at, Str::npos) << from;
        if(at != Str::npos)
            scene.replace(at, from.size(), to);

        const auto file = std::filesystem::temp_directory_path() / Str(name);
        std::ofstream(file, std::ios::binary) << scene;

        return file;
    }

    // empty handles stand in for the pool's allocations and the uploaded images
    StageBindings populateBacklot(RenderScene& scene) {
        const auto& stage = backlotStage();
        StageGeometry geometry;
        for(const auto& model: stage.models)
            geometry.models.emplace_back(model.slots.size());
        StageTextureHandles textures;
        for(const auto& material: stage.document.materials)
            textures.emplace(material.texture, TextureHandle{});

        return populateStage(scene, stage, geometry, textures);
    }

    // Backlot's inventory per lighting key
    struct KeyInventory {
        CStr key = "";
        usize instances = 0;
        u64 triangles = 0;
        usize lights = 0;
    };
}

// the rows each key shows, without a device
TEST(BacklotScene, EachKeyShowsItsInventory) {
    constexpr std::array Inventory{
        KeyInventory{.key = "day", .instances = 1328, .triangles = 466462, .lights = 0},
        KeyInventory{.key = "evening", .instances = 1439, .triangles = 469102, .lights = 31},
        KeyInventory{.key = "sunset", .instances = 1439, .triangles = 469102, .lights = 31},
        KeyInventory{.key = "night", .instances = 1488, .triangles = 470566, .lights = 31},
    };

    const auto& stage = backlotStage();
    const auto& document = stage.document;
    RenderScene scene;
    const auto bindings = populateBacklot(scene);
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

// the sign and game atlases cut at their rows' 0.5; every other quad is opaque
TEST(BacklotScene, TheSignAndGameAtlasesAreMasked) {
    RenderScene scene;
    const auto bindings = populateBacklot(scene);
    ASSERT_EQ(bindings.quadMaterials.size(), 179u);

    const auto masked = std::ranges::count_if(bindings.quadMaterials, [&](MaterialHandle handle) {
        const auto& material = scene.Materials().GetRef(handle);
        return material.pipeline.domain == MaterialDomain::Masked && material.data.alphaCutoff == 0.5f;
    });
    const auto opaque = std::ranges::count_if(bindings.quadMaterials, [&](MaterialHandle handle) {
        return scene.Materials().GetRef(handle).pipeline.domain == MaterialDomain::Opaque;
    });
    EXPECT_EQ(masked, 130);
    EXPECT_EQ(opaque, 49);
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

// the scene file read again over the launch; a scratch copy elsewhere reads
// its models and textures under the Backlot root
TEST(BacklotScene, TheSceneFileReloadsOverTheLaunch) {
    const auto& launched = backlotStage();
    const auto again = reloadStageDocument(launched, backlotRoot() / StageScenePath);
    EXPECT_EQ(again.document.instances.size(), 1488u);
    EXPECT_EQ(again.document.quads.size(), 179u);
    EXPECT_EQ(again.document.lights.size(), 31u);
    EXPECT_EQ(again.document.cameras.size(), 8u);
    EXPECT_EQ(again.document.models.size(), launched.document.models.size());
    EXPECT_EQ(again.sprites.size(), 1u);

    const auto moved = editedScene(
        "BacklotSceneTest_moved.json",
        R"("name": "lamp-ne", "area": "Street", "model": "StreetLamp", "x": 10.5,)",
        R"("name": "lamp-ne", "area": "Street", "model": "StreetLamp", "x": 11.5,)"
    );
    const auto scratch = reloadStageDocument(launched, moved);
    const auto lamp = std::ranges::find(scratch.document.instances, "lamp-ne", &StageInstance::name);
    ASSERT_NE(lamp, scratch.document.instances.end());
    EXPECT_FLOAT_EQ(lamp->position.x, 11.5f);
    EXPECT_EQ(scratch.document.root, launched.document.root);
    std::filesystem::remove(moved);

    const auto otherModel = editedScene(
        "BacklotSceneTest_refused.json",
        "Models/Street/StreetLamp.fbx",
        "Models/Street/StreetLampMoved.fbx"
    );
    try {
        reloadStageDocument(launched, otherModel);
        ADD_FAILURE() << "a model the launch did not load was accepted";
    }
    catch(const std::runtime_error& error) {
        const StrView message = error.what();
        EXPECT_NE(message.find("StreetLamp"), StrView::npos) << message;
        EXPECT_NE(message.find("restart"), StrView::npos) << message;
    }
    std::filesystem::remove(otherModel);
}
