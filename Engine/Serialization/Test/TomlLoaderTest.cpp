#include <gtest/gtest.h>
#include "TomlLoader.hpp"
#include "DOM.hpp"

using namespace Crowy;

const auto TEST_TOML1 = R"(
[metadata]
version = 0
name = "character.png"
type = "Test"

[grid]
columns = 8
rows = 4

[[animations]]
name = "idle"
start_row = 0
tile_count = 4
frame_duration_ms = 100

[[animations]]
name = "walk"
start_row = 1
tile_count = 6
frame_duration_ms = 80

[[animations]]
name = "run"
start_row = 2
tile_count = 8
frame_duration_ms = 60
)";

TEST(Serializer, SpriteTOML){
    auto v = parseTomlString(TEST_TOML1);

    if(auto* n = v.at("metadata.name")){
        if(auto s = n->asString()){
            auto str = *s;
            EXPECT_EQ(str, "character.png");
        }
        else FAIL();
    }
    else FAIL();

    if(auto* n = v.at("animations")){
        if(auto a = n->asArray()){
            auto& e1 = (*a)[0];

            if(auto t = e1.asTable()){
                for(auto& [k, v]: *t){
                    if(k=="name"){
                        if(auto val = v.asString()){
                            EXPECT_EQ(*val, "idle");
                        }
                        else FAIL();
                    }
                    else if(k=="start_row"){
                        if(auto val = v.asInt()){
                            EXPECT_EQ(*val, 0);
                        }
                        else FAIL();
                    }
                    else if(k=="tile_count"){
                        if(auto val = v.asInt()){
                            EXPECT_EQ(*val, 4);
                        }
                        else FAIL();
                    }
                    else if(k=="frame_duration_ms"){
                        if(auto val = v.asInt()){
                            EXPECT_EQ(*val, 100);
                        }
                        else FAIL();
                    }
                    else FAIL();
                }
            }
            else FAIL();
        }
        else FAIL();
    }
    else FAIL();
}

struct TestResult1{
    DocMetadata metadata;
    struct Grid{
        i64 columns = 0;
        i64 rows = 0;
    } grid;
    struct Animation{
        Str name;
        i64 startRow = 0;
        i64 tileCount = 0;
        i64 frameDurationMs = 0;
    };
    std::vector<Animation> animations;
};

template<>
struct DomTraits<TestResult1>{
    static TestResult1 from(const DOM::Value& root, const DocMetadata& metadata){
        TestResult1 result{
            .metadata = metadata
        };

        auto& grid = result.grid;
        grid.columns = root.get<i64>("grid.columns").value_or(1);
        grid.rows = root.get<i64>("grid.rows").value_or(1);

        root.forEach("animations", [&result](const DOM::Value& node){
            TestResult1::Animation animation{
                .name = node.get<Str>("name").value_or(""),
                .startRow = node.get<i64>("start_row").value_or(0),
                .tileCount = node.get<i64>("tile_count").value_or(0),
                .frameDurationMs = node.get<i64>("frame_duration_ms").value_or(0)
            };
            result.animations.push_back(std::move(animation));
        });

        return result;
    }
};

TEST(SerializerUsecase, SpriteTOML){
    auto result = loadToml<TestResult1>(TEST_TOML1);

    const auto& metadata = result.metadata;
    EXPECT_EQ(metadata.version, 0);
    EXPECT_EQ(metadata.name, "character.png");
    EXPECT_EQ(metadata.type, "Test");

    const auto& grid = result.grid;
    EXPECT_EQ(grid.columns, 8);
    EXPECT_EQ(grid.rows, 4);

    ASSERT_EQ(result.animations.size(), 3);
    const auto& anim0 = result.animations[0];
    EXPECT_EQ(anim0.name, "idle");
    EXPECT_EQ(anim0.startRow, 0);
    EXPECT_EQ(anim0.tileCount, 4);
    EXPECT_EQ(anim0.frameDurationMs, 100);

    const auto& anim1 = result.animations[1];
    EXPECT_EQ(anim1.name, "walk");
    EXPECT_EQ(anim1.startRow, 1);
    EXPECT_EQ(anim1.tileCount, 6);
    EXPECT_EQ(anim1.frameDurationMs, 80);

    const auto& anim2 = result.animations[2];
    EXPECT_EQ(anim2.name, "run");
    EXPECT_EQ(anim2.startRow, 2);
    EXPECT_EQ(anim2.tileCount, 8);
    EXPECT_EQ(anim2.frameDurationMs, 60);
}
