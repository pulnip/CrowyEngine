#include <gtest/gtest.h>
#include "DOM.hpp"
#include "JsonLoader.hpp"

using namespace Crowy;

const auto TEST_JSON1 = R"({
    "metadata": {
        "version": 0,
        "name": "character.png",
        "type": "Test"
    },
    "grid": {
        "columns": 8,
        "rows": 4
    },
    "animations": [
        {"name": "idle", "start_row": 0, "tile_count": 4, "frame_duration_ms": 100},
        {"name": "walk", "start_row": 1, "tile_count": 6, "frame_duration_ms": 80},
        {"name": "run", "start_row": 2, "tile_count": 8, "frame_duration_ms": 60}
    ]
})";

TEST(Serializer, SpriteJSON){
    auto v = parseJsonString(TEST_JSON1);

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

struct JsonTestResult1{
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
struct Crowy::DomTraits<JsonTestResult1>{
    static JsonTestResult1 from(const DOM::Value& root, const DocMetadata& metadata){
        JsonTestResult1 result{
            .metadata = metadata
        };

        auto& grid = result.grid;
        grid.columns = root.get<i64>("grid.columns").value_or(1);
        grid.rows = root.get<i64>("grid.rows").value_or(1);

        root.forEach("animations", [&result](const DOM::Value& node){
            JsonTestResult1::Animation animation{
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

TEST(SerializerUsecase, SpriteJSON){
    auto result = loadJson<JsonTestResult1>(TEST_JSON1);

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

bool deepEqual(const DOM::Value& a, const DOM::Value& b){
    using enum DOM::Value::Kind;

    if(a.kind() != b.kind()) return false;

    switch(a.kind()){
    case None:
        return true;
    case Bool:
        return *a.asBool() == *b.asBool();
    case Int:
        return *a.asInt() == *b.asInt();
    case Float:
        return *a.asFloat() == *b.asFloat();
    case String:
        return *a.asString() == *b.asString();
    case Array: {
        auto& lhs = *a.asArray();
        auto& rhs = *b.asArray();
        if(lhs.size() != rhs.size()) return false;

        for(std::size_t i = 0; i < lhs.size(); ++i){
            if(!deepEqual(lhs[i], rhs[i])) return false;
        }
        return true;
    }
    case Table: {
        auto& lhs = *a.asTable();
        auto& rhs = *b.asTable();
        if(lhs.size() != rhs.size()) return false;

        for(auto& [k, v]: lhs){
            auto it = rhs.find(k);
            if(it == rhs.end() || !deepEqual(v, it->second)) return false;
        }
        return true;
    }
    }
    return false;
}

TEST(Serializer, JsonRoundTrip){
    const auto fixture = R"({
        "flag": true,
        "count": 42,
        "ratio": 0.1,
        "name": "crowy",
        "nothing": null,
        "list": [1, 2.5, "three", [4], {"five": 5}],
        "nested": {"inner": {"leaf": -7}}
    })";
    auto v = parseJsonString(fixture);

    auto compact = parseJsonString(emitJson(v));
    EXPECT_TRUE(deepEqual(v, compact));

    auto pretty = parseJsonString(emitJson(v, JsonStyle::Pretty));
    EXPECT_TRUE(deepEqual(v, pretty));
}

TEST(Serializer, JsonParseErrors){
    EXPECT_THROW(parseJsonString("{invalid"), std::exception);
    EXPECT_THROW(parseJsonString(""), std::exception);
    EXPECT_THROW(parseJsonString(R"({"v": 9223372036854775808})"), std::out_of_range);
    EXPECT_THROW(parseJsonString("18446744073709551615"), std::out_of_range);
}

TEST(Serializer, Size2DFromPair){
    auto v = parseJsonString(R"({"pair": [8, 4], "quad": [8, 4, 2, 1], "single": [8]})");

    auto size = v.get<Size2D>("pair");
    ASSERT_TRUE(size.has_value());
    EXPECT_EQ(size->x, 8u);
    EXPECT_EQ(size->y, 4u);

    EXPECT_FALSE(v.get<Size2D>("quad").has_value());
    EXPECT_FALSE(v.get<Size2D>("single").has_value());
}

TEST(Serializer, JsonNumberStrictness){
    auto v = parseJsonString(R"({"i": 1, "f": 1.0, "big": 9223372036854775807})");

    EXPECT_TRUE(v.at("i")->asInt().has_value());
    EXPECT_FALSE(v.at("i")->asFloat().has_value());
    EXPECT_FALSE(v.at("f")->asInt().has_value());
    EXPECT_TRUE(v.at("f")->asFloat().has_value());

    EXPECT_EQ(v.get<f64>("i"), 1.0);
    EXPECT_EQ(v.get<i64>("big"), std::numeric_limits<i64>::max());
    EXPECT_EQ(v.get<i64>("f"), std::nullopt);
}
