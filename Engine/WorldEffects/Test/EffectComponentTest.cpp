#include <stdexcept>

#include <gtest/gtest.h>

#include "ClassRegistry.hpp"
#include "Component.hpp"
#include "EffectComponent.hpp"
#include "EffectSystem.hpp"
#include "FakeDevice.hpp"
#include "World.hpp"

using namespace Crowy;

namespace
{
    constexpr Vec3 Spawn{9.0f, 9.0f, 9.0f};

    ParticleEffectDesc sparks() {
        return ParticleEffectDesc{
            .name = "sparks",
            .shader = "Engine/X.slang",
            .count = 32,
            .origin = Spawn,
            .size = 0.5f
        };
    }

    Transform at(Vec3 position) {
        return Transform{.position = position};
    }

    class Fixture {
    public:
        FakeDevice device;
        EffectSystem effects{device};
        World world;

        Fixture() {
            effects.Add(sparks());
            world.Provide(effects);
        }
    };
}

// nothing here names a symbol of EffectComponent.cpp, so only linking the
// module whole keeps its component and its extraction
TEST(EffectComponent, RegistersWithoutAReference) {
    const auto* desc = ClassRegistry::FindType("EffectComponent");
    ASSERT_TRUE(desc != nullptr);
    EXPECT_TRUE(ComponentRegistry::Get().Find(*desc) != nullptr);
}

TEST(EffectComponent, TheOriginIsItsActorsPosition) {
    Fixture f;
    const auto actor = f.world.Spawn(at({1.0f, 2.0f, 3.0f}));
    f.world.Add(actor, EffectComponent{"sparks"});
    EXPECT_EQ(f.effects.Origin("sparks"), Spawn);

    f.world.Frame(0.0);
    EXPECT_EQ(f.effects.Origin("sparks"), (Vec3{1.0f, 2.0f, 3.0f}));

    f.world.SetLocalTransform(actor, at({4.0f, 5.0f, 6.0f}));
    f.world.Frame(0.0);
    EXPECT_EQ(f.effects.Origin("sparks"), (Vec3{4.0f, 5.0f, 6.0f}));
}

TEST(EffectComponent, AChildEffectRidesItsParent) {
    Fixture f;
    const auto parent = f.world.Spawn(at({0.0f, 0.8f, 0.0f}));
    f.world.Add(
        f.world.Spawn(at({0.0f, 0.45f, 0.0f}), parent),
        EffectComponent{"sparks"}
    );
    f.world.Frame(0.0);

    f.world.SetLocalTransform(parent, at({2.0f, 0.8f, 0.0f}));
    f.world.Frame(0.0);
    EXPECT_EQ(f.effects.Origin("sparks"), (Vec3{2.0f, 0.8f + 0.45f, 0.0f}));
}

TEST(EffectComponent, AFrameWhereNothingMovedWritesNoOrigin) {
    Fixture f;
    f.world.Add(f.world.Spawn(), EffectComponent{"sparks"});
    f.world.Frame(0.0);

    f.effects.SetOrigin("sparks", Spawn);
    f.world.Frame(0.0);
    EXPECT_EQ(f.effects.Origin("sparks"), Spawn);
}

TEST(EffectComponent, AnEffectNoOneAddedIsRefusedWhereTheActorGetsIt) {
    Fixture f;
    f.world.Add(f.world.Spawn(), EffectComponent{"embers"});

    EXPECT_THROW(f.world.Frame(0.0), std::invalid_argument);
}
