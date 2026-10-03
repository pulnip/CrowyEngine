#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "LinearAlgebra.hpp"
#include "PhysicsRuntime.hpp"
#include "PhysicsWorld.hpp"

using namespace Crowy;

namespace
{
    using Hashes = std::vector<u64>;

    // top face at y = 0
    BodyHandle addFloor(PhysicsWorld& world) {
        return world.CreateBody(BodyDesc{
            .shape = BoxShape{{10.0f, 0.5f, 10.0f}},
            .pose = BodyPose{.position = {0.0f, -0.5f, 0.0f}},
            .motion = BodyMotion::Static,
        });
    }

    BodyHandle addBox(PhysicsWorld& world, Vec3 position, f32 mass = 0.0f) {
        return world.CreateBody(BodyDesc{
            .shape = BoxShape{},
            .pose = BodyPose{.position = position},
            .mass = mass,
        });
    }

    void stepTimes(PhysicsWorld& world, u32 ticks) {
        for(u32 i = 0; i < ticks; ++i)
            world.Step();
    }

    // a stack, a hinged plank, a held kinematic target and a thrown ball
    void buildMixedScene(PhysicsWorld& world, f32 topBoxY = 2.7f) {
        addFloor(world);
        addBox(world, {0.0f, 0.5f, 0.0f});
        addBox(world, {0.1f, 1.6f, 0.0f});
        addBox(world, {-0.1f, topBoxY, 0.0f});

        const auto plank = world.CreateBody(BodyDesc{
            .shape = BoxShape{{0.5f, 0.05f, 0.05f}},
            .pose = BodyPose{.position = {3.5f, 1.0f, 0.0f}},
        });
        world.CreateHinge(HingeDesc{
            .body = plank,
            .pivot = {3.0f, 1.0f, 0.0f},
            .axis = unitZ(),
            .normal = unitX(),
            .minAngle = -0.5f,
            .maxAngle = 0.5f,
        });

        const auto mover = world.CreateBody(BodyDesc{
            .shape = BoxShape{{0.5f, 0.1f, 0.5f}},
            .pose = BodyPose{.position = {-3.0f, 0.1f, 0.0f}},
            .motion = BodyMotion::Kinematic,
        });
        world.MoveKinematic(
            mover,
            BodyPose{.position = {-3.0f, 1.1f, 0.0f}}
        );

        const auto ball = world.CreateBody(BodyDesc{
            .shape = SphereShape{0.25f},
            .pose = BodyPose{.position = {0.0f, 0.25f, -3.0f}},
        });
        world.AddImpulse(ball, {0.0f, 0.0f, 40.0f});
    }

    // the hash before the first tick and after each of `ticks`
    Hashes hashEveryTick(PhysicsWorld& world, u32 ticks) {
        Hashes hashes{world.StateHash()};
        for(u32 i = 0; i < ticks; ++i) {
            world.Step();
            hashes.push_back(world.StateHash());
        }

        return hashes;
    }

    Hashes runGrid(const PhysicsRuntimeDesc& desc) {
        PhysicsRuntime runtime(desc);
        PhysicsWorld world(runtime);
        addFloor(world);
        for(u32 layer = 0; layer < 2; ++layer) {
            for(u32 row = 0; row < 5; ++row) {
                for(u32 column = 0; column < 5; ++column) {
                    addBox(
                        world,
                        {
                            -2.4f + 1.2f * static_cast<f32>(column),
                            0.5f + 1.1f * static_cast<f32>(layer),
                            -2.4f + 1.2f * static_cast<f32>(row),
                        }
                    );
                }
            }
        }
        EXPECT_EQ(world.BodyCount(), 51u);

        return hashEveryTick(world, 300);
    }
}

TEST(Body, RestingBoxSettlesWithinSlop) {
    PhysicsRuntime runtime;
    PhysicsWorld world(runtime);
    addFloor(world);
    const auto box = addBox(world, {0.0f, 2.0f, 0.0f});
    EXPECT_TRUE(world.IsAwake(box));

    stepTimes(world, 120);
    // contact starts within 0.02 and penetration under the 0.02 slop stays,
    // so the box rests between its half extent and 0.02 below it
    const auto pose = world.PoseOf(box);
    EXPECT_GE(pose.position.y, 0.479f);
    EXPECT_LE(pose.position.y, 0.501f);
    EXPECT_LE(std::abs(pose.position.x), 1.0e-4f);
    EXPECT_LE(std::abs(pose.position.z), 1.0e-4f);

    std::optional<u64> asleepAt;
    while(world.TickCount() < 300) {
        world.Step();
        if(!asleepAt && !world.IsAwake(box))
            asleepAt = world.TickCount();
    }
    EXPECT_TRUE(asleepAt.has_value());
    EXPECT_FALSE(world.IsAwake(box));
    EXPECT_EQ(world.BodyCount(), 2u);
    EXPECT_EQ(world.TickCount(), 300u);
}

TEST(Body, StaticPoseReadsBackBitForBit) {
    PhysicsRuntime runtime;
    PhysicsWorld world(runtime);
    const auto pose = BodyPose{
        .position = {1.25f, -0.5f, 3.0f},
        .rotation = {0.0f, 0.38268343f, 0.0f, 0.92387953f},
    };
    const auto body = world.CreateBody(BodyDesc{
        .shape = BoxShape{{0.5f, 0.25f, 1.0f}},
        .pose = pose,
        .motion = BodyMotion::Static,
    });

    for(const auto ticks: {0u, 10u}) {
        stepTimes(world, ticks);
        const auto read = world.PoseOf(body);
        EXPECT_EQ(read.position, pose.position);
        EXPECT_EQ(read.rotation, pose.rotation);
        EXPECT_EQ(world.LinearVelocityOf(body), zeros());
        EXPECT_FALSE(world.IsAwake(body));
    }
    EXPECT_EQ(world.TickCount(), 10u);
}

TEST(Body, FreeFallFollowsGravity) {
    PhysicsRuntime runtime;
    PhysicsWorld world(runtime);
    const auto ball = world.CreateBody(BodyDesc{
        .shape = SphereShape{0.5f},
        .pose = BodyPose{.position = {0.0f, 10.0f, 0.0f}},
        .linearDamping = 0.0f,
    });

    stepTimes(world, 60);
    // sixty float additions of dt * g, each off by under 5e-7
    const auto velocity = world.LinearVelocityOf(ball);
    EXPECT_NEAR(velocity.y, -9.81f, 1.0e-4f);
    EXPECT_EQ(velocity.x, 0.0f);
    EXPECT_EQ(velocity.z, 0.0f);
    EXPECT_EQ(world.TickCount(), 60u);
}

TEST(Body, MassFromShapeOrDesc) {
    PhysicsRuntime runtime;
    PhysicsWorld world(runtime);
    const auto cube = addBox(world, {0.0f, 0.0f, 0.0f});
    const auto heavy = addBox(world, {3.0f, 0.0f, 0.0f}, 2.0f);
    const auto ball = world.CreateBody(BodyDesc{
        .shape = SphereShape{0.5f},
        .pose = BodyPose{.position = {-3.0f, 0.0f, 0.0f}},
    });

    EXPECT_EQ(world.MassOf(cube), 1000.0f);
    EXPECT_EQ(world.MassOf(heavy), 2.0f);
    EXPECT_NEAR(world.MassOf(ball), 523.599f, 0.01f);
    EXPECT_EQ(world.BodyCount(), 3u);
}

TEST(Body, ImpulseWakesSleeper) {
    PhysicsRuntime runtime;
    PhysicsWorld world(runtime);
    addFloor(world);
    const auto box = addBox(world, {0.0f, 0.5f, 0.0f}, 2.0f);

    while(world.IsAwake(box) && world.TickCount() < 300)
        world.Step();
    ASSERT_FALSE(world.IsAwake(box));

    // sleeping zeroed the velocity, and 10 * 1/2 is exact
    world.AddImpulse(box, {0.0f, 0.0f, 10.0f});
    EXPECT_TRUE(world.IsAwake(box));
    EXPECT_EQ(world.LinearVelocityOf(box), (Vec3{0.0f, 0.0f, 5.0f}));
}

TEST(Body, ImpulseAtAddsSpin) {
    PhysicsRuntime runtime;
    PhysicsWorld world(runtime, PhysicsWorldDesc{.gravity = zeros()});
    const auto plain = addBox(world, {-2.0f, 0.0f, 0.0f}, 1.0f);
    const auto spun = addBox(world, {2.0f, 0.0f, 0.0f}, 1.0f);

    world.AddImpulse(plain, {0.0f, 0.0f, 1.0f});
    world.AddImpulseAt(spun, {0.0f, 0.0f, 1.0f}, {2.5f, 0.0f, 0.0f});
    EXPECT_EQ(world.LinearVelocityOf(plain), (Vec3{0.0f, 0.0f, 1.0f}));
    EXPECT_EQ(world.LinearVelocityOf(spun), (Vec3{0.0f, 0.0f, 1.0f}));

    stepTimes(world, 10);
    // no spin skips the rotation step entirely
    EXPECT_EQ(world.PoseOf(plain).rotation, unitQuat());
    // r x J = (0, -0.5, 0) over I = 1/6 is -3 rad/s: about -0.5 rad by now
    const auto rotation = world.PoseOf(spun).rotation;
    EXPECT_GE(rotation.y, -0.26f);
    EXPECT_LE(rotation.y, -0.23f);
    EXPECT_LE(std::abs(rotation.x), 1.0e-5f);
    EXPECT_LE(std::abs(rotation.z), 1.0e-5f);
}

TEST(Body, KinematicReachesAndHolds) {
    PhysicsRuntime runtime;
    PhysicsWorld world(runtime);
    const auto kinematic = BodyDesc{
        .shape = BoxShape{},
        .motion = BodyMotion::Kinematic,
    };
    const auto mover = world.CreateBody(kinematic);
    auto idleDesc = kinematic;
    idleDesc.pose.position = {0.0f, 0.0f, 5.0f};
    const auto idle = world.CreateBody(idleDesc);

    world.MoveKinematic(mover, BodyPose{.position = {1.0f, 0.0f, 0.0f}});
    world.Step();
    EXPECT_NEAR(world.PoseOf(mover).position.x, 1.0f, 1.0e-6f);
    // the velocity that got it there is still set
    EXPECT_NEAR(world.LinearVelocityOf(mover).x, 60.0f, 1.0e-3f);

    for(u32 tick = 2; tick <= 10; ++tick) {
        world.Step();
        EXPECT_NEAR(world.PoseOf(mover).position.x, 1.0f, 1.0e-6f);
        EXPECT_LE(norm(world.LinearVelocityOf(mover)), 1.0e-5f);
    }
    EXPECT_EQ(world.PoseOf(idle).position, idleDesc.pose.position);
    EXPECT_EQ(world.LinearVelocityOf(idle), zeros());
    EXPECT_EQ(world.TickCount(), 10u);
}

TEST(Hinge, StopsAtLimit) {
    PhysicsRuntime runtime;
    const auto plank = BodyDesc{
        .shape = BoxShape{{0.5f, 0.05f, 0.05f}},
        .pose = BodyPose{.position = {0.5f, 1.0f, 0.0f}},
    };
    auto hingeAt = [&](PhysicsWorld& world, f32 limit) {
        return world.CreateHinge(HingeDesc{
            .body = world.CreateBody(plank),
            .pivot = {0.0f, 1.0f, 0.0f},
            .axis = unitZ(),
            .normal = unitX(),
            .minAngle = -limit,
            .maxAngle = limit,
        });
    };

    PhysicsWorld limited(runtime);
    const auto hinge = hingeAt(limited, 0.5f);
    EXPECT_NEAR(limited.HingeAngleOf(hinge), 0.0f, 1.0e-6f);
    stepTimes(limited, 300);
    // gravity swings +x toward -y: negative about +z, held at the limit
    EXPECT_GE(limited.HingeAngleOf(hinge), -0.52f);
    EXPECT_LE(limited.HingeAngleOf(hinge), -0.48f);

    PhysicsWorld free(runtime);
    const auto freeHinge = hingeAt(free, 3.14159f);
    f32 lowest = 0.0f;
    for(u32 tick = 0; tick < 60; ++tick) {
        free.Step();
        lowest = std::min(lowest, free.HingeAngleOf(freeHinge));
    }
    EXPECT_LT(lowest, -1.0f);
}

TEST(Query, OverlapSeesSleepersSkipsStatics) {
    PhysicsRuntime runtime;
    PhysicsWorld world(runtime);
    addFloor(world);
    // created out of spatial order, so the result's order is creation's
    const auto right = addBox(world, {2.0f, 0.5f, 0.0f});
    const auto left = addBox(world, {-2.0f, 0.5f, 0.0f});
    const auto middle = addBox(world, {0.0f, 0.5f, 0.0f});
    const auto mover = world.CreateBody(BodyDesc{
        .shape = BoxShape{},
        .pose = BodyPose{.position = {4.0f, 0.5f, 0.0f}},
        .motion = BodyMotion::Kinematic,
    });

    stepTimes(world, 120);
    for(const auto body: {right, left, middle})
        ASSERT_FALSE(world.IsAwake(body));

    const auto found = world.Overlapping({0.0f, 0.25f, 0.0f}, {5.0f, 0.5f, 1.0f});
    EXPECT_EQ(found, (BodyHandles{right, left, middle, mover}));
    EXPECT_TRUE(world.Overlapping({0.0f, 5.0f, 0.0f}, {1.0f, 1.0f, 1.0f}).empty());
    for(const auto body: {right, left, middle})
        EXPECT_FALSE(world.IsAwake(body));
}

TEST(Hash, RepeatsPerTick) {
    PhysicsRuntime runtime;
    PhysicsWorld first(runtime);
    PhysicsWorld second(runtime);
    buildMixedScene(first);
    buildMixedScene(second);

    const auto firstHashes = hashEveryTick(first, 300);
    const auto secondHashes = hashEveryTick(second, 300);
    EXPECT_EQ(firstHashes.size(), 301u);
    EXPECT_EQ(firstHashes, secondHashes);
}

TEST(Hash, SeesOneUlp) {
    PhysicsRuntime runtime;
    PhysicsWorld reference(runtime);
    PhysicsWorld nudged(runtime);
    buildMixedScene(reference);
    buildMixedScene(nudged, std::nextafter(2.7f, 3.0f));

    EXPECT_NE(reference.StateHash(), nudged.StateHash());
    reference.Step();
    nudged.Step();
    EXPECT_NE(reference.StateHash(), nudged.StateHash());
}

TEST(Hash, IgnoresWorkerCount) {
    const auto alone = runGrid(PhysicsRuntimeDesc{.workerThreads = 0});
    const auto shared = runGrid(PhysicsRuntimeDesc{.workerThreads = 3});
    EXPECT_EQ(alone.size(), 301u);
    EXPECT_EQ(alone, shared);
}

TEST(Runtime, TwiceInOneProcess) {
    auto runScene = [] {
        PhysicsRuntime runtime;
        PhysicsWorld world(runtime);
        buildMixedScene(world);
        return hashEveryTick(world, 60);
    };

    const auto first = runScene();
    const auto second = runScene();
    EXPECT_EQ(first.size(), 61u);
    EXPECT_EQ(first, second);
}

TEST(Handle, ScopedToItsWorld) {
    PhysicsRuntime runtime;
    PhysicsWorld one(runtime);
    PhysicsWorld other(runtime);
    const auto desc = BodyDesc{
        .shape = SphereShape{0.25f},
        .motion = BodyMotion::Kinematic,
    };
    const auto mine = one.CreateBody(desc);
    const auto theirs = other.CreateBody(desc);
    const auto hinged = one.CreateBody(BodyDesc{});
    const auto hinge = one.CreateHinge(HingeDesc{.body = hinged});

    EXPECT_FALSE(one.IsValid(BodyHandle{}));
    EXPECT_FALSE(one.IsValid(HingeHandle{}));
    EXPECT_TRUE(one.IsValid(mine));
    EXPECT_FALSE(one.IsValid(theirs));
    EXPECT_TRUE(other.IsValid(theirs));
    EXPECT_FALSE(other.IsValid(mine));
    EXPECT_TRUE(one.IsValid(hinge));
    EXPECT_FALSE(other.IsValid(hinge));
    EXPECT_FALSE(one.IsValid(
        BodyHandle{mine.GetIndex(), mine.GetGeneration() + 1}
    ));

    EXPECT_EQ(one.MotionOf(mine), BodyMotion::Kinematic);
    ASSERT_TRUE(std::holds_alternative<SphereShape>(one.ShapeOf(mine)));
    EXPECT_EQ(std::get<SphereShape>(one.ShapeOf(mine)).radius, 0.25f);
    EXPECT_EQ(one.BodyCount(), 2u);
    EXPECT_EQ(other.BodyCount(), 1u);
}

#if defined(_DEBUG) || !defined(NDEBUG)
TEST(PhysicsDeathTest, SecondRuntime) {
    EXPECT_DEATH(
        {
            PhysicsRuntime first;
            PhysicsRuntime second;
        },
        "already exists"
    );
}

TEST(PhysicsDeathTest, RuntimeOutlivedByWorld) {
    EXPECT_DEATH(
        {
            auto runtime = std::make_unique<PhysicsRuntime>();
            PhysicsWorld world(*runtime);
            runtime.reset();
        },
        "still has 1 worlds"
    );
}

TEST(PhysicsDeathTest, ForeignHandle) {
    EXPECT_DEATH(
        {
            PhysicsRuntime runtime;
            PhysicsWorld one(runtime);
            PhysicsWorld other(runtime);
            const auto body = other.CreateBody(BodyDesc{});
            one.PoseOf(body);
        },
        "not a body of this world"
    );
}

TEST(PhysicsDeathTest, ImpulseOnStatic) {
    EXPECT_DEATH(
        {
            PhysicsRuntime runtime;
            PhysicsWorld world(runtime);
            world.AddImpulse(addFloor(world), {0.0f, 1.0f, 0.0f});
        },
        "Dynamic"
    );
}

TEST(PhysicsDeathTest, MoveKinematicOnDynamic) {
    EXPECT_DEATH(
        {
            PhysicsRuntime runtime;
            PhysicsWorld world(runtime);
            world.MoveKinematic(world.CreateBody(BodyDesc{}), BodyPose{});
        },
        "Kinematic"
    );
}

TEST(PhysicsDeathTest, HingeLimitOutsideRange) {
    EXPECT_DEATH(
        {
            PhysicsRuntime runtime;
            PhysicsWorld world(runtime);
            world.CreateHinge(HingeDesc{
                .body = world.CreateBody(BodyDesc{}),
                .minAngle = 0.7f,
            });
        },
        "minAngle"
    );
}

TEST(PhysicsDeathTest, WorldFull) {
    EXPECT_DEATH(
        {
            PhysicsRuntime runtime;
            PhysicsWorld world(runtime, PhysicsWorldDesc{.maxBodies = 1});
            world.CreateBody(BodyDesc{});
            world.CreateBody(BodyDesc{});
        },
        "maxBodies"
    );
}
#endif
