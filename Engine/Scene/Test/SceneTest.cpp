#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "ActorRef.hpp"
#include "ClassRegistry.hpp"
#include "Component.hpp"
#include "JsonLoader.hpp"
#include "Scene.hpp"
#include "World.hpp"

using namespace Crowy;

enum class SceneFinish : u8 {
    Matte,
    Gloss,
};

namespace
{
    struct ScenePaint {
        Vec3 color = ones();
        f32 strength = 1.0f;
        SceneFinish finish = SceneFinish::Matte;
    };

    struct SceneLink {
        ActorRef target;
    };

    struct SceneRuntime {
        i32 frames = 0;
    };

    struct SceneOpaque {
        i32 hidden = 0;
    };

    // a lamp on a stand, and a camera linked to the lamp; the camera meets
    // its types in the other order than their names sort
    void buildStage(World& world) {
        const auto stand =
            world.Spawn(Transform{.position = {0.0f, 1.0f, 0.0f}});
        world.SetName(stand, "stand");
        const auto lamp =
            world.Spawn(Transform{.position = {0.5f, 0.0f, 0.0f}}, stand);
        world.SetName(lamp, "lamp");
        const auto camera =
            world.Spawn(Transform{.position = {0.0f, 2.0f, -4.0f}});
        world.SetName(camera, "camera");

        world.Add(
            lamp,
            ScenePaint{
                .color = {1.0f, 0.5f, 0.25f},
                .strength = 2.0f,
                .finish = SceneFinish::Gloss
            }
        );
        world.Add(lamp, SceneRuntime{});
        world.Add(camera, SceneLink{.target = {lamp}});
        world.Add(camera, ScenePaint{});
        world.Frame(0.0);
    }

    std::vector<ActorId> actorsOf(const World& world) {
        std::vector<ActorId> actors;
        world.EachActor([&](ActorId actor, ActorId) {
            actors.push_back(actor);
        });

        return actors;
    }

    template<typename F>
    Str failureOf(F&& f) {
        try {
            f();
        }
        catch(const std::runtime_error& error) {
            return error.what();
        }
        return "";
    }

    constexpr auto StageText =
        R"({"metadata":{"version":1,"type":"scene"},"actors":[)"
        R"({"id":1,"name":"stand","parent":null,"transform":{)"
        R"("position":[0.0,1.0,0.0],"rotation":[0.0,0.0,0.0,1.0],)"
        R"("scale":[1.0,1.0,1.0]},"components":[]},)"
        R"({"id":2,"name":"lamp","parent":1,"transform":{)"
        R"("position":[0.5,0.0,0.0],"rotation":[0.0,0.0,0.0,1.0],)"
        R"("scale":[1.0,1.0,1.0]},"components":[)"
        R"({"type":"ScenePaint","color":[1.0,0.5,0.25],"strength":2.0,)"
        R"("finish":"Gloss"}]},)"
        R"({"id":3,"name":"camera","parent":null,"transform":{)"
        R"("position":[0.0,2.0,-4.0],"rotation":[0.0,0.0,0.0,1.0],)"
        R"("scale":[1.0,1.0,1.0]},"components":[)"
        R"({"type":"SceneLink","target":2},)"
        R"({"type":"ScenePaint","color":[1.0,1.0,1.0],"strength":1.0,)"
        R"("finish":"Matte"}]}]})";
}

namespace Crowy
{
    // clang-format off
    CROWY_ENUM_BEGIN(SceneFinish)
        CROWY_ENUM_VALUE(Matte)
        CROWY_ENUM_VALUE(Gloss)
    CROWY_ENUM_END()

    CROWY_COMPONENT(ScenePaint)
        .AddProperty("color", &ScenePaint::color)
        .AddProperty("strength", &ScenePaint::strength)
        .AddProperty("finish", &ScenePaint::finish)
    CROWY_COMPONENT_END(ScenePaint)

    CROWY_COMPONENT(SceneLink)
        .AddProperty("target", &SceneLink::target)
    CROWY_COMPONENT_END(SceneLink)

    CROWY_COMPONENT(SceneRuntime)
        .Transient()
    CROWY_COMPONENT_END(SceneRuntime)

    CROWY_COMPONENT(SceneOpaque)
    CROWY_COMPONENT_END(SceneOpaque)
    // clang-format on
}

TEST(Scene, ASavedWorldIsItsActorsParentsFirst) {
    World world;
    buildStage(world);

    EXPECT_EQ(emitJson(saveScene(world)), StageText);
}

TEST(Scene, ALoadedSceneSavesToTheSameBytes) {
    World world;
    loadScene(world, parseJsonString(StageText));
    world.Frame(0.0);

    EXPECT_EQ(emitJson(saveScene(world)), StageText);
}

TEST(Scene, ALoadKeepsNamesParentsPlacesAndReferences) {
    World world;
    loadScene(world, parseJsonString(StageText));
    world.Frame(0.0);

    const auto actors = actorsOf(world);
    ASSERT_EQ(actors.size(), 3u);
    const auto lamp = actors[1];
    EXPECT_EQ(world.Name(lamp), "lamp");
    EXPECT_EQ(world.FileId(lamp), 2);
    EXPECT_EQ(world.WorldPosition(lamp), (Vec3{0.5f, 1.0f, 0.0f}));
    EXPECT_EQ(world.Get<ScenePaint>(lamp)->finish, SceneFinish::Gloss);
    EXPECT_EQ(world.Get<SceneRuntime>(lamp), nullptr);

    const auto camera = actors[2];
    EXPECT_EQ(world.Get<SceneLink>(camera)->target.actor, lamp);
}

TEST(Scene, AnEditRenumbersNoOtherActor) {
    World world;
    loadScene(world, parseJsonString(StageText));
    world.Frame(0.0);

    world.Destroy(actorsOf(world)[1]);
    world.Spawn();
    world.Frame(0.0);

    const auto saved = saveScene(world);
    std::vector<i64> ids;
    for(const auto& actor: *saved.asTable()->find("actors")->second.asArray())
        ids.push_back(*actor.get<i64>("id"));
    EXPECT_EQ(ids, (std::vector<i64>{1, 3, 4}));
    // the lamp left the file, so the link names no actor in it
    EXPECT_TRUE(saved.at("actors[1].components[0].target")->is_none());
}

TEST(Scene, AComponentWithNoPropertiesIsNotSaved) {
    World world;
    world.Add(world.Spawn(), SceneOpaque{});
    world.Frame(0.0);

    EXPECT_EQ(
        failureOf([&] { saveScene(world); }),
        "the scene cannot be saved:\n"
        "actor 1: 'SceneOpaque' has no properties to save; reflect them or "
        "mark it Transient"
    );
}

TEST(Scene, ABrokenSceneLandsNoActor) {
    struct Broken {
        CStr actors;
        CStr message;
    };
    const Broken cases[] = {
        {R"({"id":1,"components":[{"type":"Nope"}]})",
         "actor 1: 'Nope' is not a component"},
        {R"({"id":1,"components":[{"type":"ScenePaint","shine":1}]})",
         "actor 1: ScenePaint: no property 'shine' on 'ScenePaint'"},
        {R"({"id":1,"components":[{"type":"SceneRuntime"}]})",
         "actor 1: 'SceneRuntime' is never saved, so no file gives it"},
        {R"({"id":1,"parent":2},{"id":2})",
         "actor 1: the parent is no actor before it"},
        {R"({"id":1},{"id":1})", "actors[1]: id 1 is given twice"},
        {R"({"id":1,"colour":[1,0,0]})", "actors[0] has no key 'colour'"},
    };

    for(const auto& [actors, message]: cases) {
        SCOPED_TRACE(actors);
        World world;
        const auto text = std::format(
            R"({{"metadata":{{"version":1,"type":"scene"}},"actors":[{}]}})",
            actors
        );

        EXPECT_EQ(
            failureOf([&] { loadScene(world, parseJsonString(text)); }),
            std::format("the scene cannot be loaded:\n{}", message)
        );
        world.Frame(0.0);
        EXPECT_TRUE(actorsOf(world).empty());
    }
}

TEST(Scene, AnotherVersionIsNotRead) {
    World world;
    const auto text =
        R"({"metadata":{"version":2,"type":"scene"},"actors":[]})";

    EXPECT_EQ(
        failureOf([&] { loadScene(world, parseJsonString(text)); }),
        "a scene of version 2 cannot be read; this reads version 1"
    );
}

TEST(Scene, AFileReadsBackAsTheSameScene) {
    World world;
    buildStage(world);
    const auto path =
        std::filesystem::temp_directory_path() / "CrowySceneTest.scene.json";
    saveSceneFile(world, path);

    std::ifstream file(path, std::ios::binary);
    const Str text{
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    };
    file.close();
    EXPECT_EQ(text, emitJson(saveScene(world), JsonStyle::Pretty) + "\n");

    World loaded;
    loadSceneFile(loaded, path);
    loaded.Frame(0.0);
    std::filesystem::remove(path);

    EXPECT_EQ(emitJson(saveScene(loaded)), StageText);
}
