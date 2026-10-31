#include <vector>

#include <gtest/gtest.h>

#include "FakeDevice.hpp"
#include "FixedTickClock.hpp"
#include "GameSession.hpp"
#include "GeometryPool.hpp"
#include "JsonLoader.hpp"
#include "MeshGenerator.hpp"
#include "PathUtil.hpp"
#include "System.hpp"

using namespace Crowy;

namespace
{
    constexpr auto BoxScene = R"({
    "metadata": {"version": 1, "type": "scene"},
    "actors": [
        {
            "id": 1,
            "name": "camera",
            "parent": null,
            "transform": {
                "position": [0.0, 1.0, -4.0],
                "rotation": [0.0, 0.0, 0.0, 1.0],
                "scale": [1.0, 1.0, 1.0]
            },
            "components": [
                {"type": "CameraComponent", "fovY": 1.0, "nearZ": 0.5, "farZ": 50.0}
            ]
        },
        {
            "id": 2,
            "name": "box",
            "parent": null,
            "transform": {
                "position": [0.0, 0.0, 0.0],
                "rotation": [0.0, 0.0, 0.0, 1.0],
                "scale": [1.0, 1.0, 1.0]
            },
            "components": [
                {
                    "type": "MeshComponent",
                    "mesh": "engine:Builtin/Box",
                    "materials": ["engine:Builtin/Opaque"],
                    "visible": true,
                    "castShadow": true
                }
            ]
        }
    ]
})";

    const f64 OneTick = static_cast<f64>(TickNanoseconds) * 1e-9;

    std::vector<FrameTime> updates;
    std::vector<u64> ticks;

    void recordUpdate(World&, const FrameTime& time) {
        updates.push_back(time);
    }

    void recordTick(World&, const FrameTime& time) {
        ticks.push_back(time.tick);
    }

    class GameSessionTest: public testing::Test {
    protected:
        FakeDevice device;
        AssetRoots roots{
            .engine = toPath(CROWY_ENGINE_ROOT),
            .content = toPath(CROWY_ENGINE_ROOT)
        };

        void SetUp() override {
            updates.clear();
            ticks.clear();
        }
    };
}

namespace Crowy
{
    // clang-format off
    CROWY_SYSTEM(SessionTestUpdate, Update, &recordUpdate)
    CROWY_SYSTEM_END(SessionTestUpdate)

    CROWY_SYSTEM(SessionTestTick, PreFixedUpdate, &recordTick)
    CROWY_SYSTEM_END(SessionTestTick)
    // clang-format on
}

TEST_F(GameSessionTest, ALoadedSceneIsDrawableBeforeTheFirstFrame) {
    const auto box = MakeBox(0.5f);
    GeometryPool pool{
        device,
        static_cast<u32>(box.vertices.size()),
        static_cast<u32>(box.indices.size())
    };
    GameSession session(device, pool, roots);
    session.Load(parseJsonString(BoxScene));

    EXPECT_EQ(session.Render().Scene().Primitives().Count(), 1u);
    EXPECT_EQ(session.Camera().nearZ, 0.5f);
    EXPECT_EQ(session.Camera().farZ, 50.0f);
}

TEST_F(GameSessionTest, DestroyingASessionFreesTheWorldTheSceneAndTheStores) {
    const auto box = MakeBox(0.5f);
    GeometryPool pool{
        device,
        static_cast<u32>(box.vertices.size()),
        static_cast<u32>(box.indices.size())
    };
    const auto addBox = [&] {
        return pool.Add(box.vertices, box.indices);
    };
    {
        GameSession session(device, pool, roots);
        session.Load(parseJsonString(BoxScene));
        ASSERT_EQ(session.Render().Meshes().Count(), 1u);
        ASSERT_EQ(session.Render().Materials().Count(), 1u);
        EXPECT_FALSE(addBox().has_value());
    }

    device.RunDeferred();
    EXPECT_TRUE(addBox().has_value());
}

TEST_F(GameSessionTest, TheScaleReachesTheWorldsGameTimeAndNotItsRealTime) {
    GeometryPool pool{device, 64, 256};
    GameSession session(device, pool, roots);
    updates.clear();

    session.SetTimeScale(0.5);
    session.Frame(0.2);
    session.SetTimeScale(2.0);
    session.Frame(0.2);

    ASSERT_EQ(updates.size(), 2u);
    EXPECT_DOUBLE_EQ(updates[0].gameSeconds, 0.1);
    EXPECT_DOUBLE_EQ(updates[1].gameSeconds, 0.4);
    EXPECT_EQ(updates[0].realSeconds, 0.2);
    EXPECT_EQ(updates[1].realSeconds, 0.2);
}

TEST_F(GameSessionTest, ThePauseStopsTheGameClockAndItsFixedTicks) {
    GeometryPool pool{device, 64, 256};
    GameSession session(device, pool, roots);
    session.Frame(2.0 * OneTick);
    ASSERT_EQ(ticks, (std::vector<u64>{1, 2}));
    updates.clear();

    session.SetPaused(true);
    session.Frame(1.0);
    ASSERT_EQ(updates.size(), 1u);
    EXPECT_EQ(updates[0].gameSeconds, 0.0);
    EXPECT_EQ(updates[0].realSeconds, 1.0);
    EXPECT_EQ(ticks.size(), 2u);
    EXPECT_EQ(session.GetWorld().FrameTicks(), 0u);

    session.SetPaused(false);
    session.Frame(OneTick);
    EXPECT_EQ(ticks, (std::vector<u64>{1, 2, 3}));
}

TEST_F(GameSessionTest, TheFixedTicksAreOwedByTheScaledClock) {
    GeometryPool pool{device, 64, 256};
    GameSession session(device, pool, roots);

    session.SetTimeScale(2.0);
    session.Frame(OneTick);
    EXPECT_EQ(ticks, (std::vector<u64>{1, 2}));
}
