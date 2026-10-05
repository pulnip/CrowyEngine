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
        world.MoveKinematic(mover, BodyPose{.position = {-3.0f, 1.1f, 0.0f}});

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
        // every component non-zero, so a swapped or negated slot shows
        .rotation = {0.1f, 0.2f, 0.3f, 0.92736185f},
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
    EXPECT_EQ(world.BodyCount(), 1u);
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
    EXPECT_EQ(world.BodyCount(), 1u);
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
    EXPECT_EQ(world.TickCount(), 0u);
    EXPECT_EQ(world.BodyCount(), 3u);
}

TEST(Body, ImpulseWakesSleeper) {
    PhysicsRuntime runtime;
    PhysicsWorld world(runtime);
    addFloor(world);
    const auto box = addBox(world, {0.0f, 0.5f, 0.0f}, 2.0f);

    stepTimes(world, 300);
    ASSERT_FALSE(world.IsAwake(box));

    // sleeping zeroed the velocity, and 10 * 1/2 is exact
    world.AddImpulse(box, {0.0f, 0.0f, 10.0f});
    EXPECT_TRUE(world.IsAwake(box));
    EXPECT_EQ(world.LinearVelocityOf(box), (Vec3{0.0f, 0.0f, 5.0f}));
    EXPECT_EQ(world.TickCount(), 300u);
    EXPECT_EQ(world.BodyCount(), 2u);
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
    EXPECT_EQ(world.TickCount(), 10u);
    EXPECT_EQ(world.BodyCount(), 2u);
}

TEST(Body, MaterialReachesJolt) {
    PhysicsRuntime runtime;
    auto highestRise = [&](f32 restitution) {
        PhysicsWorld world(runtime);
        addFloor(world);
        const auto ball = world.CreateBody(BodyDesc{
            .shape = SphereShape{0.25f},
            .pose = BodyPose{.position = {0.0f, 2.0f, 0.0f}},
            .restitution = restitution,
        });
        f32 rise = 0.0f;
        for(u32 tick = 0; tick < 120; ++tick) {
            world.Step();
            rise = std::max(rise, world.LinearVelocityOf(ball).y);
        }
        EXPECT_EQ(world.TickCount(), 120u);
        EXPECT_EQ(world.BodyCount(), 2u);
        return rise;
    };
    // restitution combines by max, so the ball's alone decides the bounce
    EXPECT_GT(highestRise(1.0f), 4.0f);
    EXPECT_LT(highestRise(0.0f), 0.5f);

    auto speedAfterSliding = [&](f32 friction) {
        PhysicsWorld world(runtime);
        addFloor(world);
        const auto box = world.CreateBody(BodyDesc{
            .shape = BoxShape{},
            .pose = BodyPose{.position = {-5.0f, 0.5f, 0.0f}},
            .mass = 1.0f,
            .friction = friction,
            .linearDamping = 0.0f,
        });
        world.AddImpulse(box, {2.0f, 0.0f, 0.0f});
        stepTimes(world, 60);
        EXPECT_EQ(world.BodyCount(), 2u);
        return world.LinearVelocityOf(box).x;
    };
    // friction combines by geometric mean: 0 with anything is frictionless
    EXPECT_GT(speedAfterSliding(0.0f), 1.95f);
    // 0.2 decelerates at about 1.96 m/s^2, so 2 m/s is gone within a second
    EXPECT_LT(speedAfterSliding(0.2f), 0.1f);

    auto turnedAngle = [&](f32 angularDamping) {
        PhysicsWorld world(runtime, PhysicsWorldDesc{.gravity = zeros()});
        const auto ball = world.CreateBody(BodyDesc{
            .shape = SphereShape{0.5f},
            .angularDamping = angularDamping,
        });
        world.AddImpulseAt(ball, {0.0f, 0.0f, 100.0f}, {0.5f, 0.0f, 0.0f});
        stepTimes(world, 60);
        EXPECT_EQ(world.TickCount(), 60u);
        // w = cos(angle / 2), so a smaller turn leaves a larger w
        return world.PoseOf(ball).rotation.w;
    };
    EXPECT_GT(turnedAngle(2.0f), turnedAngle(0.0f) + 0.05f);
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
    EXPECT_EQ(world.BodyCount(), 2u);
}

TEST(Hinge, StopsAtLimit) {
    PhysicsRuntime runtime;
    const auto plank = BodyDesc{
        .shape = BoxShape{{0.5f, 0.05f, 0.05f}},
        .pose = BodyPose{.position = {0.5f, 1.0f, 0.0f}},
    };
    auto hingeAt = [&](PhysicsWorld& world, f32 limit, f32 friction) {
        return world.CreateHinge(HingeDesc{
            .body = world.CreateBody(plank),
            .pivot = {0.0f, 1.0f, 0.0f},
            .axis = unitZ(),
            .normal = unitX(),
            .minAngle = -limit,
            .maxAngle = limit,
            .maxFrictionTorque = friction,
        });
    };

    PhysicsWorld limited(runtime);
    const auto hinge = hingeAt(limited, 0.5f, 0.0f);
    EXPECT_NEAR(limited.HingeAngleOf(hinge), 0.0f, 1.0e-6f);
    stepTimes(limited, 300);
    // gravity swings +x toward -y: negative about +z, held at the limit
    EXPECT_GE(limited.HingeAngleOf(hinge), -0.52f);
    EXPECT_LE(limited.HingeAngleOf(hinge), -0.48f);
    EXPECT_EQ(limited.TickCount(), 300u);
    EXPECT_EQ(limited.BodyCount(), 1u);

    auto lowestOver60 = [](PhysicsWorld& world, HingeHandle swinging) {
        f32 lowest = 0.0f;
        for(u32 tick = 0; tick < 60; ++tick) {
            world.Step();
            lowest = std::min(lowest, world.HingeAngleOf(swinging));
        }
        return lowest;
    };

    PhysicsWorld free(runtime);
    const auto freeHinge = hingeAt(free, 3.14159f, 0.0f);
    EXPECT_LT(lowestOver60(free, freeHinge), -1.0f);
    EXPECT_EQ(free.TickCount(), 60u);

    // the 10 kg plank pulls with about 49 N m; friction outweighs it
    PhysicsWorld stiff(runtime);
    const auto stiffHinge = hingeAt(stiff, 3.14159f, 200.0f);
    EXPECT_GT(lowestOver60(stiff, stiffHinge), -0.05f);
    EXPECT_EQ(stiff.TickCount(), 60u);
    EXPECT_EQ(stiff.BodyCount(), 1u);
}

namespace
{
    // an open-top box: a floor and four walls around its origin
    CompoundShape openBox(f32 halfWidth, f32 wallHeight) {
        constexpr f32 Thickness = 0.05f;
        const auto wallY = Thickness + 0.5f * wallHeight;
        const auto wallHalfY = 0.5f * wallHeight;
        const auto side = halfWidth - Thickness;

        return CompoundShape{{
            CompoundPart{
                .halfExtent = {halfWidth, Thickness, halfWidth},
                .pose = BodyPose{.position = {0.0f, 0.0f, 0.0f}},
            },
            CompoundPart{
                .halfExtent = {Thickness, wallHalfY, halfWidth},
                .pose = BodyPose{.position = {-side, wallY, 0.0f}},
            },
            CompoundPart{
                .halfExtent = {Thickness, wallHalfY, halfWidth},
                .pose = BodyPose{.position = {side, wallY, 0.0f}},
            },
            CompoundPart{
                .halfExtent = {halfWidth, wallHalfY, Thickness},
                .pose = BodyPose{.position = {0.0f, wallY, -side}},
            },
            CompoundPart{
                .halfExtent = {halfWidth, wallHalfY, Thickness},
                .pose = BodyPose{.position = {0.0f, wallY, side}},
            },
        }};
    }
}

TEST(Compound, CupHoldsABallAndWeighsItsBoxes) {
    PhysicsRuntime runtime;
    PhysicsWorld world(runtime);
    const auto cup = world.CreateBody(BodyDesc{
        .shape = openBox(0.5f, 0.3f),
        .motion = BodyMotion::Static,
    });
    const auto ball = world.CreateBody(BodyDesc{
        .shape = SphereShape{0.1f},
        .pose = BodyPose{.position = {0.2f, 1.0f, -0.1f}},
    });
    // the same boxes, dynamic: 1000 kg/m^3 over their summed volume
    const auto heavy = world.CreateBody(BodyDesc{
        .shape = openBox(0.5f, 0.3f),
        .pose = BodyPose{.position = {5.0f, 0.0f, 0.0f}},
        .motion = BodyMotion::Dynamic,
    });

    stepTimes(world, 120);
    const auto rest = world.PoseOf(ball).position;
    // on the cup's floor, whose top is at 0.05, sunk at most the 0.02 slop,
    // and inside its walls
    EXPECT_GE(rest.y, 0.125f);
    EXPECT_LE(rest.y, 0.151f);
    EXPECT_LT(std::abs(rest.x), 0.4f);
    EXPECT_LT(std::abs(rest.z), 0.4f);
    ASSERT_TRUE(std::holds_alternative<CompoundShape>(world.ShapeOf(cup)));
    EXPECT_EQ(std::get<CompoundShape>(world.ShapeOf(cup)).parts.size(), 5u);
    const auto volume = 2.0f * 0.05f * 1.0f * 1.0f +
        2.0f * (0.1f * 0.3f * 1.0f) + 2.0f * (1.0f * 0.3f * 0.1f);
    EXPECT_NEAR(world.MassOf(heavy), 1000.0f * volume, 0.5f);
    EXPECT_EQ(world.TickCount(), 120u);
    EXPECT_EQ(world.BodyCount(), 3u);
}

// the ball is never pushed; moving its container carries it along
TEST(Compound, MovedTrayCarriesTheBall) {
    PhysicsRuntime runtime;
    PhysicsWorld world(runtime);
    const auto tray = world.CreateBody(BodyDesc{
        .shape = openBox(0.4f, 0.2f),
        .motion = BodyMotion::Kinematic,
    });
    const auto ball = world.CreateBody(BodyDesc{
        .shape = SphereShape{0.08f},
        .pose = BodyPose{.position = {0.0f, 0.15f, 0.0f}},
        .friction = 0.5f,
    });

    stepTimes(world, 30);
    // 0.5 m along x over 2 s, in literal steps of 1/240 m a tick
    Vec3 target = zeros();
    for(u32 tick = 0; tick < 120; ++tick) {
        target.x += 0.5f / 120.0f;
        world.MoveKinematic(tray, BodyPose{.position = target});
        world.Step();
    }
    stepTimes(world, 60);
    const auto trayX = world.PoseOf(tray).position.x;
    const auto ballX = world.PoseOf(ball).position.x;
    EXPECT_NEAR(trayX, 0.5f, 1.0e-4f);
    EXPECT_GT(ballX, 0.2f);
    EXPECT_LT(std::abs(ballX - trayX), 0.32f);
    EXPECT_EQ(world.TickCount(), 210u);
    EXPECT_EQ(world.BodyCount(), 2u);
}

// a hinge disabled wakes nothing by itself; the release must
TEST(Hinge, ReleaseWakesASleeper) {
    PhysicsRuntime runtime;
    PhysicsWorld world(runtime);
    const auto bob = world.CreateBody(BodyDesc{
        .shape = SphereShape{0.1f},
        .pose = BodyPose{.position = {0.0f, 1.0f, 0.0f}},
    });
    const auto hinge = world.CreateHinge(HingeDesc{
        .body = bob,
        .pivot = {0.0f, 2.0f, 0.0f},
        .axis = unitZ(),
        .normal = unitX(),
    });

    // hanging straight down, it falls asleep
    u32 ticks = 0;
    while(world.IsAwake(bob) && ticks < 120) {
        world.Step();
        ++ticks;
    }
    ASSERT_FALSE(world.IsAwake(bob));

    world.ReleaseHinge(hinge);
    EXPECT_TRUE(world.IsAwake(bob));
    stepTimes(world, 30);
    EXPECT_LT(world.PoseOf(bob).position.y, 0.9f);
    EXPECT_EQ(world.TickCount(), ticks + 30u);
    EXPECT_EQ(world.BodyCount(), 1u);
}

TEST(Hinge, ReleaseFreesTheBody) {
    PhysicsRuntime runtime;
    PhysicsWorld world(runtime);
    const auto bob = world.CreateBody(BodyDesc{
        .shape = SphereShape{0.1f},
        .pose = BodyPose{.position = {1.0f, 2.0f, 0.0f}},
        .linearDamping = 0.0f,
    });
    const auto hinge = world.CreateHinge(HingeDesc{
        .body = bob,
        .pivot = {0.0f, 2.0f, 0.0f},
        .axis = unitZ(),
        .normal = unitX(),
    });

    // held, the bob stays one arm's length from the pivot
    stepTimes(world, 30);
    const auto held = world.PoseOf(bob).position;
    EXPECT_NEAR(norm(held - Vec3{0.0f, 2.0f, 0.0f}), 1.0f, 0.01f);

    EXPECT_TRUE(world.IsHingeHeld(hinge));
    world.ReleaseHinge(hinge);
    EXPECT_FALSE(world.IsHingeHeld(hinge));
    stepTimes(world, 60);
    // released, it flies off the circle
    const auto free = world.PoseOf(bob).position;
    EXPECT_GT(norm(free - Vec3{0.0f, 2.0f, 0.0f}), 2.0f);
    EXPECT_TRUE(world.IsValid(hinge));
    EXPECT_EQ(world.TickCount(), 90u);
    EXPECT_EQ(world.BodyCount(), 1u);
}

TEST(Hinge, ReleaseShowsInTheHash) {
    PhysicsRuntime runtime;
    auto pendulum = [](PhysicsWorld& world) {
        return world.CreateHinge(HingeDesc{
            .body = world.CreateBody(BodyDesc{
                .shape = SphereShape{0.1f},
                .pose = BodyPose{.position = {1.0f, 2.0f, 0.0f}},
            }),
            .pivot = {0.0f, 2.0f, 0.0f},
            .axis = unitZ(),
            .normal = unitX(),
        });
    };
    PhysicsWorld held(runtime);
    PhysicsWorld released(runtime);
    pendulum(held);
    const auto hinge = pendulum(released);

    for(u32 tick = 0; tick < 30; ++tick) {
        held.Step();
        released.Step();
        ASSERT_EQ(held.StateHash(), released.StateHash());
    }
    released.ReleaseHinge(hinge);
    held.Step();
    released.Step();
    EXPECT_NE(held.StateHash(), released.StateHash());
    EXPECT_EQ(released.TickCount(), 31u);
    EXPECT_EQ(released.BodyCount(), 1u);
}

TEST(Compound, OnePartKeepsItsOffset) {
    PhysicsRuntime runtime;
    PhysicsWorld world(runtime);
    addFloor(world);
    // one part becomes a rotated-translated shape in Jolt
    world.CreateBody(BodyDesc{
        .shape = CompoundShape{{CompoundPart{
            .halfExtent = {0.3f, 0.1f, 0.3f},
            .pose = BodyPose{.position = {0.5f, 1.0f, 0.0f}},
        }}},
        .motion = BodyMotion::Static,
    });
    const auto onPart = addBox(world, {0.5f, 2.0f, 0.0f});
    const auto offPart = addBox(world, {-0.5f, 2.0f, 0.0f});

    stepTimes(world, 120);
    // the part's top is at 1.1; the floor's at 0
    EXPECT_GT(world.PoseOf(onPart).position.y, 1.5f);
    EXPECT_LT(world.PoseOf(offPart).position.y, 0.6f);
    EXPECT_EQ(world.TickCount(), 120u);
    EXPECT_EQ(world.BodyCount(), 4u);
}

TEST(Compound, TurnsAboutItsOrigin) {
    PhysicsRuntime runtime;
    PhysicsWorld world(runtime, PhysicsWorldDesc{.gravity = zeros()});
    const auto arm = world.CreateBody(BodyDesc{
        .shape = CompoundShape{{CompoundPart{
            .halfExtent = {0.2f, 0.05f, 0.05f},
            .pose = BodyPose{.position = {1.0f, 0.0f, 0.0f}},
        }}},
        .pose = BodyPose{.position = {2.0f, 1.0f, 3.0f}},
        .motion = BodyMotion::Kinematic,
    });

    // a quarter turn about y
    world.MoveKinematic(
        arm,
        BodyPose{
            .position = {2.0f, 1.0f, 3.0f},
            .rotation = {0.0f, 0.70710677f, 0.0f, 0.70710677f},
        }
    );
    stepTimes(world, 30);
    const auto pose = world.PoseOf(arm);
    EXPECT_NEAR(pose.position.x, 2.0f, 1.0e-5f);
    EXPECT_NEAR(pose.position.y, 1.0f, 1.0e-5f);
    EXPECT_NEAR(pose.position.z, 3.0f, 1.0e-5f);
    EXPECT_NEAR(pose.rotation.y, 0.70710677f, 1.0e-5f);
    EXPECT_NEAR(pose.rotation.w, 0.70710677f, 1.0e-5f);
    EXPECT_EQ(world.TickCount(), 30u);
    EXPECT_EQ(world.BodyCount(), 1u);
}

// one ramp, half frictionless: a block on that half slides, the other holds
TEST(Compound, PartsKeepTheirFriction) {
    // a 20 degree slope down toward -x
    constexpr Vec4 Slope{0.0f, 0.0f, 0.17364818f, 0.98480775f};
    constexpr f32 Sin = 0.34202014f;
    constexpr f32 Cos = 0.93969262f;

    PhysicsRuntime runtime;
    PhysicsWorld world(runtime);
    world.CreateBody(BodyDesc{
        .shape = CompoundShape{{
            CompoundPart{
                .halfExtent = {1.0f, 0.05f, 0.25f},
                .pose = BodyPose{.position = {0.0f, 0.0f, -0.3f}},
            },
            CompoundPart{
                .halfExtent = {1.0f, 0.05f, 0.25f},
                .pose = BodyPose{.position = {0.0f, 0.0f, 0.3f}},
                .friction = 0.0f,
            },
        }},
        .pose = BodyPose{.position = {0.0f, 1.0f, 0.0f}, .rotation = Slope},
        .motion = BodyMotion::Static,
        .friction = 1.0f,
    });
    // resting on the slope, 0.15 m along its normal
    auto blockOn = [&](f32 z) {
        return world.CreateBody(BodyDesc{
            .shape = BoxShape{{0.1f, 0.1f, 0.1f}},
            .pose = BodyPose{
                .position = {-0.15f * Sin, 1.0f + 0.15f * Cos, z},
                .rotation = Slope,
            },
            .friction = 1.0f,
        });
    };
    const auto held = blockOn(-0.3f);
    const auto slid = blockOn(0.3f);
    const auto heldStart = world.PoseOf(held).position;
    const auto slidStart = world.PoseOf(slid).position;

    stepTimes(world, 30);
    // tan 20 degrees is 0.36, under the held half's friction of 1
    EXPECT_LT(norm(world.PoseOf(held).position - heldStart), 0.01f);
    // g sin 20 degrees for half a second: 0.42 m
    const auto slide = slidStart - world.PoseOf(slid).position;
    EXPECT_GT(slide.x, 0.3f);
    EXPECT_GT(slide.y, 0.1f);
    EXPECT_EQ(world.TickCount(), 30u);
    EXPECT_EQ(world.BodyCount(), 3u);
}

// a block across the seam of two touching parts feels both, whichever part
// is listed first
TEST(Compound, ASeamFeelsBothParts) {
    constexpr Vec4 Slope{0.0f, 0.0f, 0.17364818f, 0.98480775f};
    constexpr f32 Sin = 0.34202014f;
    constexpr f32 Cos = 0.93969262f;

    u32 held = 0;
    for(const bool frictionlessFirst: {true, false}) {
        SCOPED_TRACE(frictionlessFirst);
        PhysicsRuntime runtime;
        PhysicsWorld world(runtime);
        auto slick = CompoundPart{
            .halfExtent = {1.0f, 0.05f, 0.25f},
            .pose = BodyPose{.position = {0.0f, 0.0f, -0.25f}},
            .friction = 0.0f,
        };
        auto rough = CompoundPart{
            .halfExtent = {1.0f, 0.05f, 0.25f},
            .pose = BodyPose{.position = {0.0f, 0.0f, 0.25f}},
        };
        world.CreateBody(BodyDesc{
            .shape = frictionlessFirst ? CompoundShape{{slick, rough}}
                                       : CompoundShape{{rough, slick}},
            .pose = BodyPose{.position = {0.0f, 1.0f, 0.0f}, .rotation = Slope},
            .motion = BodyMotion::Static,
            .friction = 1.0f,
        });
        const auto block = world.CreateBody(BodyDesc{
            .shape = BoxShape{{0.1f, 0.1f, 0.1f}},
            .pose = BodyPose{
                .position = {-0.15f * Sin, 1.0f + 0.15f * Cos, 0.0f},
                .rotation = Slope,
            },
            .friction = 1.0f,
        });
        const auto start = world.PoseOf(block).position;

        stepTimes(world, 30);
        // the rough half alone holds half the weight at friction 1, more
        // than the 0.34 tan 20 degrees asks for
        if(norm(world.PoseOf(block).position - start) < 0.01f)
            ++held;
        EXPECT_EQ(world.TickCount(), 30u);
        EXPECT_EQ(world.BodyCount(), 2u);
    }
    EXPECT_EQ(held, 2u);
}

TEST(Water, LightFloatsHeavySinks) {
    PhysicsRuntime runtime;
    PhysicsWorld world(runtime);
    addFloor(world);
    // surface at y = 2
    world.AddWater(WaterDesc{
        .center = {0.0f, 1.0f, 0.0f},
        .halfExtent = {3.0f, 1.0f, 3.0f},
    });
    // 0.2 m spheres at 300 and 3000 kg/m^3 (the volume is 0.0335 m^3)
    const auto ballAt = [&](f32 x, f32 mass) {
        return world.CreateBody(BodyDesc{
            .shape = SphereShape{0.2f},
            .pose = BodyPose{.position = {x, 1.0f, 0.0f}},
            .mass = mass,
        });
    };
    const auto light = ballAt(-1.0f, 10.053f);
    const auto heavy = ballAt(1.0f, 100.53f);

    stepTimes(world, 300);
    // floating with about 30 % of it under the surface
    EXPECT_GT(world.PoseOf(light).position.y, 1.9f);
    EXPECT_LT(world.PoseOf(light).position.y, 2.2f);
    // on the floor, top at y = 0
    EXPECT_NEAR(world.PoseOf(heavy).position.y, 0.2f, 0.03f);
    EXPECT_EQ(world.TickCount(), 300u);
    EXPECT_EQ(world.BodyCount(), 3u);
}

// pushed deeper, a float accelerates for longer and rises higher
TEST(Water, DeeperRisesHigher) {
    PhysicsRuntime runtime;
    auto peakFrom = [&](f32 depth) {
        PhysicsWorld world(runtime);
        addFloor(world);
        world.AddWater(WaterDesc{
            .center = {0.0f, 1.0f, 0.0f},
            .halfExtent = {3.0f, 1.0f, 3.0f},
        });
        const auto buoy = world.CreateBody(BodyDesc{
            .shape = SphereShape{0.2f},
            .pose = BodyPose{.position = {0.0f, 2.0f - depth, 0.0f}},
            .mass = 10.053f,
        });
        f32 peak = 0.0f;
        for(u32 tick = 0; tick < 120; ++tick) {
            world.Step();
            peak = std::max(peak, world.PoseOf(buoy).position.y);
        }
        EXPECT_EQ(world.TickCount(), 120u);
        EXPECT_EQ(world.BodyCount(), 2u);
        return peak;
    };
    EXPECT_GT(peakFrom(0.6f), peakFrom(0.15f) + 0.2f);
}

TEST(Water, OnlyInsideTheBox) {
    PhysicsRuntime runtime;
    PhysicsWorld world(runtime);
    addFloor(world);
    world.AddWater(WaterDesc{
        .center = {0.0f, 1.0f, 0.0f},
        .halfExtent = {1.0f, 1.0f, 1.0f},
    });
    // below the surface's height, beside the water
    const auto outside = world.CreateBody(BodyDesc{
        .shape = SphereShape{0.2f},
        .pose = BodyPose{.position = {3.0f, 1.0f, 0.0f}},
        .mass = 10.053f,
    });

    stepTimes(world, 120);
    EXPECT_NEAR(world.PoseOf(outside).position.y, 0.2f, 0.03f);
    EXPECT_EQ(world.TickCount(), 120u);
    EXPECT_EQ(world.BodyCount(), 2u);
}

TEST(Water, InertWithoutBodies) {
    PhysicsRuntime runtime;
    PhysicsWorld dry(runtime);
    PhysicsWorld wet(runtime);
    for(auto* world: {&dry, &wet}) {
        addFloor(*world);
        addBox(*world, {0.0f, 2.0f, 0.0f});
    }
    // nothing ever enters it
    wet.AddWater(WaterDesc{
        .center = {5.0f, 1.0f, 0.0f},
        .halfExtent = {1.0f, 1.0f, 1.0f},
    });

    for(u32 tick = 0; tick < 60; ++tick) {
        dry.Step();
        wet.Step();
        ASSERT_EQ(dry.StateHash(), wet.StateHash());
    }
    EXPECT_EQ(wet.TickCount(), 60u);
    EXPECT_EQ(wet.BodyCount(), 2u);
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

    const auto found =
        world.Overlapping({0.0f, 0.25f, 0.0f}, {5.0f, 0.5f, 1.0f});
    EXPECT_EQ(found, (BodyHandles{right, left, middle, mover}));
    EXPECT_TRUE(
        world.Overlapping({0.0f, 5.0f, 0.0f}, {1.0f, 1.0f, 1.0f}).empty()
    );
    for(const auto body: {right, left, middle})
        EXPECT_FALSE(world.IsAwake(body));
    EXPECT_EQ(world.TickCount(), 120u);
    EXPECT_EQ(world.BodyCount(), 5u);
}

TEST(Hash, RepeatsPerTick) {
    PhysicsRuntime runtime;
    PhysicsWorld first(runtime);
    PhysicsWorld second(runtime);
    buildMixedScene(first);
    buildMixedScene(second);

    EXPECT_EQ(first.BodyCount(), 7u);
    EXPECT_EQ(second.BodyCount(), 7u);

    const auto firstHashes = hashEveryTick(first, 300);
    const auto secondHashes = hashEveryTick(second, 300);
    EXPECT_EQ(first.TickCount(), 300u);
    EXPECT_EQ(second.TickCount(), 300u);
    EXPECT_EQ(firstHashes, secondHashes);
}

TEST(Hash, SeesOneUlp) {
    PhysicsRuntime runtime;
    PhysicsWorld reference(runtime);
    PhysicsWorld nudged(runtime);
    buildMixedScene(reference);
    buildMixedScene(nudged, std::nextafter(2.7f, 3.0f));

    EXPECT_EQ(reference.BodyCount(), 7u);
    EXPECT_EQ(nudged.BodyCount(), 7u);

    EXPECT_NE(reference.StateHash(), nudged.StateHash());
    reference.Step();
    nudged.Step();
    EXPECT_NE(reference.StateHash(), nudged.StateHash());
    EXPECT_EQ(reference.TickCount(), 1u);
    EXPECT_EQ(nudged.TickCount(), 1u);
}

TEST(Hash, SeesTheTick) {
    PhysicsRuntime runtime;
    PhysicsWorld world(runtime);
    addFloor(world);

    // nothing is active, so Jolt's state stays put and only the tick moves
    const auto hashes = hashEveryTick(world, 2);
    EXPECT_NE(hashes[0], hashes[1]);
    EXPECT_NE(hashes[1], hashes[2]);
    EXPECT_EQ(world.TickCount(), 2u);
    EXPECT_EQ(world.BodyCount(), 1u);
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
        EXPECT_EQ(world.BodyCount(), 7u);
        auto hashes = hashEveryTick(world, 60);
        EXPECT_EQ(world.TickCount(), 60u);
        return hashes;
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
    const auto hinge = one.CreateHinge(HingeDesc{.body = one.CreateBody({})});
    const auto theirHinge =
        other.CreateHinge(HingeDesc{.body = other.CreateBody({})});

    EXPECT_FALSE(one.IsValid(BodyHandle{}));
    EXPECT_FALSE(one.IsValid(HingeHandle{}));
    EXPECT_TRUE(one.IsValid(mine));
    EXPECT_FALSE(one.IsValid(theirs));
    EXPECT_TRUE(other.IsValid(theirs));
    EXPECT_FALSE(other.IsValid(mine));
    // both hinge tables hold slot 0, so only the world tag tells them apart
    EXPECT_TRUE(one.IsValid(hinge));
    EXPECT_FALSE(other.IsValid(hinge));
    EXPECT_TRUE(other.IsValid(theirHinge));
    EXPECT_FALSE(one.IsValid(theirHinge));
    EXPECT_FALSE(one.IsValid(
        BodyHandle{mine.GetIndex(), mine.GetGeneration() + 1}
    ));

    EXPECT_EQ(one.MotionOf(mine), BodyMotion::Kinematic);
    ASSERT_TRUE(std::holds_alternative<SphereShape>(one.ShapeOf(mine)));
    EXPECT_EQ(std::get<SphereShape>(one.ShapeOf(mine)).radius, 0.25f);
    EXPECT_EQ(one.BodyCount(), 2u);
    EXPECT_EQ(other.BodyCount(), 2u);
    EXPECT_EQ(one.TickCount(), 0u);
    EXPECT_EQ(other.TickCount(), 0u);
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

TEST(PhysicsDeathTest, EmptyCompound) {
    EXPECT_DEATH(
        {
            PhysicsRuntime runtime;
            PhysicsWorld world(runtime);
            world.CreateBody(BodyDesc{.shape = CompoundShape{}});
        },
        "a compound needs a part"
    );
}

TEST(Water, RestingBodiesSleep) {
    PhysicsRuntime runtime;
    PhysicsWorld world(runtime);
    addFloor(world);
    world.AddWater(WaterDesc{
        .center = {0.0f, 1.0f, 0.0f},
        .halfExtent = {1.0f, 1.0f, 1.0f},
    });
    // three times the water's density: it sinks to the floor and rests
    const auto stone = world.CreateBody(BodyDesc{
        .shape = SphereShape{0.2f},
        .pose = BodyPose{.position = {0.0f, 1.0f, 0.0f}},
        .mass = 3000.0f * 4.0f / 3.0f * 3.1415927f * 0.008f,
    });

    stepTimes(world, 300);
    EXPECT_NEAR(world.PoseOf(stone).position.y, 0.2f, 0.025f);
    EXPECT_FALSE(world.IsAwake(stone));
    EXPECT_EQ(world.TickCount(), 300u);
    EXPECT_EQ(world.BodyCount(), 2u);
}

TEST(PhysicsDeathTest, NegativePartFriction) {
    EXPECT_DEATH(
        {
            PhysicsRuntime runtime;
            PhysicsWorld world(runtime);
            world.CreateBody(BodyDesc{
                .shape = CompoundShape{{CompoundPart{.friction = -0.1f}}},
            });
        },
        "a part's friction is not negative"
    );
}

TEST(PhysicsDeathTest, WaterWithoutVolume) {
    EXPECT_DEATH(
        {
            PhysicsRuntime runtime;
            PhysicsWorld world(runtime);
            world.AddWater(WaterDesc{.halfExtent = {1.0f, 0.0f, 1.0f}});
        },
        "positive half extent"
    );
}

TEST(PhysicsDeathTest, ReleaseForeignHinge) {
    EXPECT_DEATH(
        {
            PhysicsRuntime runtime;
            PhysicsWorld one(runtime);
            PhysicsWorld other(runtime);
            const auto hinge =
                other.CreateHinge(HingeDesc{.body = other.CreateBody({})});
            one.ReleaseHinge(hinge);
        },
        "not a hinge of this world"
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
