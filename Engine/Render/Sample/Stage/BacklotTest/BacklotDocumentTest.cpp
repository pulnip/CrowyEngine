#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "BacklotContent.hpp"
#include "ModelLoader.hpp"
#include "StageDocument.hpp"

using namespace Crowy;

namespace
{
    const StageDocument& document() {
        static const auto loaded = loadStageDocument(backlotRoot());

        return loaded;
    }

    template<typename T>
    const T* named(const std::vector<T>& rows, StrView name) {
        for(const auto& row: rows) {
            if(row.name == name)
                return &row;
        }

        return nullptr;
    }
}

TEST(BacklotDocument, SectionsMatchTheirTotals) {

    const auto& stage = document();
    EXPECT_EQ(stage.version, 1u);
    EXPECT_EQ(stage.models.size(), 124u);
    EXPECT_EQ(stage.materials.size(), 10u);
    EXPECT_EQ(stage.instances.size(), 1488u);
    EXPECT_EQ(stage.quads.size(), 179u);
    EXPECT_EQ(stage.lights.size(), 31u);
    EXPECT_EQ(stage.cameras.size(), 8u);
    ASSERT_EQ(stage.lightingKeys.size(), 4u);
    EXPECT_EQ(stage.defaultKey, "day");

    const std::vector<Str> keyNames{"day", "evening", "sunset", "night"};
    for(usize i = 0; i < keyNames.size(); ++i)
        EXPECT_EQ(stage.lightingKeys[i].name, keyNames[i]);
}

// rows the contract documents, read back as written
TEST(BacklotDocument, KnownRowsReadAsWritten) {
    const auto& stage = document();

    const auto* block = named(stage.instances, "block-nw");
    ASSERT_NE(block, nullptr);
    EXPECT_EQ(block->model, "CornerBlock");
    EXPECT_FLOAT_EQ(block->position.x, -25.25f);
    EXPECT_FLOAT_EQ(block->position.z, 25.25f);
    EXPECT_FLOAT_EQ(block->yaw, 270.0f);

    const auto* ground = named(stage.instances, "ground");
    ASSERT_NE(ground, nullptr);
    EXPECT_EQ(ground->model, "RoadSlab");
    EXPECT_FLOAT_EQ(ground->position.y, -0.2f);
    EXPECT_EQ(ground->emissiveChannel, DefaultEmissiveChannel);
    EXPECT_TRUE(ground->keys.empty());

    const auto* lamp = named(stage.lights, "street-lamps-lamp-ne");
    ASSERT_NE(lamp, nullptr);
    EXPECT_EQ(lamp->kind, StageLightKind::Spot);
    EXPECT_EQ(lamp->group, "street_lamps");
    EXPECT_FLOAT_EQ(lamp->range, 16.0f);
    EXPECT_FLOAT_EQ(lamp->innerAngle, 40.0f);
    EXPECT_FLOAT_EQ(lamp->outerAngle, 60.0f);
    EXPECT_FLOAT_EQ(lamp->position.x, 10.5f);
    EXPECT_FLOAT_EQ(lamp->direction.y, -1.0f);
    EXPECT_EQ(lamp->shadowPriority, 1u);

    const auto* plan = named(stage.cameras, "plan");
    ASSERT_NE(plan, nullptr);
    EXPECT_TRUE(plan->orthographic);
    EXPECT_FLOAT_EQ(plan->orthoSize, 52.0f);
    EXPECT_FLOAT_EQ(plan->pitch, -90.0f);

    const auto* screen = named(stage.quads, "screen-ne");
    ASSERT_NE(screen, nullptr);
    ASSERT_TRUE(screen->flipbook.has_value());
    EXPECT_EQ(screen->flipbook->animation, "loop");
    EXPECT_FLOAT_EQ(screen->uv1.x, 0.5f);
    EXPECT_FLOAT_EQ(screen->uv1.y, 0.25f);

    // its first frame is the quad's own rect
    const auto sprite = loadStageSprite(stage, screen->flipbook->sprite);
    EXPECT_EQ(sprite.rows, 4u);
    EXPECT_EQ(sprite.columns, 2u);
    ASSERT_EQ(sprite.animations.size(), 1u);
    EXPECT_EQ(sprite.animations[0].frameCount, 8u);
    EXPECT_EQ(sprite.animations[0].frameDurationMs, 160u);
    EXPECT_FLOAT_EQ(1.0f / static_cast<f32>(sprite.columns), screen->uv1.x - screen->uv0.x);
    EXPECT_FLOAT_EQ(1.0f / static_cast<f32>(sprite.rows), screen->uv1.y - screen->uv0.y);

    const auto& day = stage.lightingKeys.front();
    EXPECT_EQ(day.lightGroups.size(), 6u);
    for(const auto& group: day.lightGroups)
        EXPECT_FLOAT_EQ(group.scale, 0.0f);
    EXPECT_EQ(day.emissiveChannels.size(), 8u);
    EXPECT_NEAR(day.sunDirection.y, -0.788f, 1e-4f);
}

// the final inventory's per-area and per-key counts
TEST(BacklotDocument, AreasAndKeyedRowsMatchTheInventory) {
    const auto& stage = document();

    std::map<Str, usize> perArea;
    for(const auto& instance: stage.instances)
        ++perArea[instance.area];
    EXPECT_EQ(perArea["Centre"], 209u);
    EXPECT_EQ(perArea["Edges"], 10u);
    EXPECT_EQ(perArea["NE"], 367u);
    EXPECT_EQ(perArea["NW"], 337u);
    EXPECT_EQ(perArea["SE"], 167u);
    EXPECT_EQ(perArea["SW"], 144u);
    EXPECT_EQ(perArea["Street"], 254u);

    const auto shows = [](const std::vector<Str>& keys, StrView key) {
        return keys.empty() || std::ranges::find(keys, key) != keys.end();
    };
    const std::vector<std::pair<Str, usize>> expected{
        {"day", 1328}, {"evening", 1439}, {"sunset", 1439}, {"night", 1488}
    };
    for(const auto& [key, count]: expected) {
        const auto instances = std::ranges::count_if(stage.instances, [&](const StageInstance& row) {
            return shows(row.keys, key);
        });
        const auto quads = std::ranges::count_if(stage.quads, [&](const StageQuad& row) {
            return shows(row.keys, key);
        });
        EXPECT_EQ(static_cast<usize>(instances), count) << key;
        EXPECT_EQ(quads, 161) << key;
    }
}

// the engine's loader test reads a copy of this probe
TEST(BacklotDocument, EngineFixtureIsBacklotsProbe) {
    const auto probe = backlotRoot() / "Unity/Assets/Art/Models/Probe/AxisProbe.fbx";
    const auto read = [](const std::filesystem::path& file) {
        std::ifstream stream(file, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(stream), {});
    };
    EXPECT_EQ(read(probe), read(toPath(CROWY_ENGINE_PROBE)));
}

TEST(BacklotDocument, PaletteCarriesItsDetailBlock) {
    const auto& stage = document();

    const StageMaterial* palette = nullptr;
    for(const auto& material: stage.materials) {
        if(material.id == "Palette")
            palette = &material;
    }
    ASSERT_NE(palette, nullptr);
    EXPECT_EQ(palette->sampler, StageSampler::Point);
    ASSERT_TRUE(palette->detail.has_value());
    EXPECT_FLOAT_EQ(palette->detail->metersPerTile, 2.0f);
}

// a row's size is a card's nominal size, which a model may miss by 9 %: the
// bar is what the conversion controls, no axis swapped and grounded
TEST(BacklotDocument, EveryModelImportsAtItsSize) {
    const auto& stage = document();

    const auto near = [](f32 imported, f32 nominal) {
        return std::abs(imported - nominal) <= std::max(0.06f, 0.1f * nominal);
    };
    for(const auto& row: stage.models) {
        const auto model = loadModel(resolveStagePath(stage, row.path));
        const auto size = 2.0f * model.bounds.halfScale;
        EXPECT_TRUE(near(size.x, row.size.x)) << row.id << " x " << size.x << " vs " << row.size.x;
        EXPECT_TRUE(near(size.y, row.size.y)) << row.id << " y " << size.y << " vs " << row.size.y;
        EXPECT_TRUE(near(size.z, row.size.z)) << row.id << " z " << size.z << " vs " << row.size.z;

        // where x and z differ enough to tell apart, the import fits its
        // row better than the same import with x and z swapped would
        if(std::abs(row.size.x - row.size.z) > 0.2f * std::max(row.size.x, row.size.z)) {
            const auto fit = std::abs(size.x - row.size.x) + std::abs(size.z - row.size.z);
            const auto swapped = std::abs(size.z - row.size.x) + std::abs(size.x - row.size.z);
            EXPECT_LT(fit, swapped) << row.id;
        }

        // grounded: the lowest point at y = 0
        EXPECT_NEAR(model.bounds.center.y - model.bounds.halfScale.y, 0.0f, 0.005f) << row.id;

        // a material the file declares but no face uses has no slot
        ASSERT_FALSE(model.slots.empty()) << row.id;
        for(const auto& slot: model.slots)
            EXPECT_NE(std::ranges::find(row.materials, slot.material), row.materials.end())
                << row.id << " draws with '" << slot.material << "'";
    }
}
