#include <array>
#include <bit>
#include <cstring>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "EffectSystem.hpp"
#include "FakeCommandList.hpp"
#include "FakeDevice.hpp"
#include "LinearAlgebra.hpp"
#include "ParticleEffects.hpp"
#include "PipelineCache.hpp"
#include "RHICommandList.hpp"

using namespace Crowy;

namespace
{
    ParticleEffectDesc desc(CStr name, u32 count, u32 prewarmSteps = 0) {
        return ParticleEffectDesc{
            .name = name,
            .shader = "Engine/X.slang",
            .count = count,
            .seed = 5,
            .prewarmSteps = prewarmSteps,
            .draws = {{.entry = "glow"}}
        };
    }

    EffectPush pushOf(const FakeCommandList::Bytes& bytes) {
        EffectPush push;
        std::memcpy(&push, bytes.data(), sizeof(push));

        return push;
    }

    class Fixture {
    public:
        FakeDevice device;
        FakeCommandList cmdList;
        FakeTexture color{RHIPixelFormat::RGBA16_FLOAT, 4, 4, 0xC0};
        FakeBuffer view{256};
        PipelineCache pipelines{device};
        EffectSystem effects{device};
        // the world step the last frame showed
        u32 worldStep = 0;

        // a frame of `steps` steps: the simulation, then a pass drawing the
        // effects the way a hook pass does, after view 0; returns the draws
        u32 Frame(u32 steps = 1) {
            static constexpr std::array ColorFormats{
                RHIPixelFormat::RGBA16_FLOAT
            };

            cmdList.Begin();
            worldStep += steps;
            const auto releases =
                effects.Simulate(cmdList, EffectView{}, worldStep, steps);
            const std::array colors{RHIColorAttachment{.texture = &color}};
            const std::array acquires{MakeBarrier(
                color,
                RHIResourceUsage::Undefined,
                RHIResourceUsage::RenderTarget
            )};
            cmdList.BeginRenderPass(
                RHIRenderPassDesc{.colorAttachments = colors},
                acquires,
                releases
            );
            cmdList.SetGraphicsConstantBuffer(view, 0);
            const auto draws = effects.Draw(
                cmdList,
                HookPassContext{
                    .pass = "Effects",
                    .formats =
                        HookPassFormats{
                            .colors = ColorFormats,
                            .depth = RHIPixelFormat::D32_FLOAT
                        }
                },
                pipelines
            );
            cmdList.EndRenderPass();
            cmdList.Close();

            return draws;
        }
    };
}

TEST(EffectSystem, RefusesUnnamedEmptyDuplicateOrOversizedEffects) {
    FakeDevice device;
    EffectSystem effects(device);
    EXPECT_THROW(effects.Add(desc("", 8)), std::invalid_argument);
    EXPECT_THROW(effects.Add(desc("none", 0)), std::invalid_argument);
    EXPECT_THROW(
        effects.Add(desc("huge", MaxDispatchThreads + 1)),
        std::invalid_argument
    );
    auto unnamedDraw = desc("draw", 8);
    unnamedDraw.draws[0].entry.clear();
    EXPECT_THROW(effects.Add(unnamedDraw), std::invalid_argument);

    effects.Add(desc("sparks", 8));
    EXPECT_THROW(effects.Add(desc("sparks", 8)), std::invalid_argument);
    EXPECT_EQ(device.bufferCreates.size(), 1u);
    EXPECT_EQ(device.bufferCreates[0].size, 8u * sizeof(EffectParticle));
    ASSERT_EQ(device.computeCreates.size(), 2u);
    EXPECT_EQ(device.computeCreates[1].computeShader.entryPoint, "cs_step");
}

// the first frame steps each effect through its prewarm behind barriers;
// the second steps each once, acquiring across the submission
TEST(EffectSystem, TheFirstFrameRunsThePrewarm) {
    Fixture f;
    f.effects.Add(desc("a", 256, 3));
    f.effects.Add(desc("b", 128));

    f.Frame();
    EXPECT_TRUE(f.cmdList.violations.empty());
    EXPECT_TRUE(f.cmdList.unconsumedBuffersAtClose.empty());
    ASSERT_EQ(f.cmdList.computePasses.size(), 1u);
    const auto& first = f.cmdList.computePasses[0];
    ASSERT_EQ(first.dispatches.size(), 5u);
    const std::array steps{0u, 1u, 2u, 3u, 0u};
    const std::array threads{256u, 256u, 256u, 256u, 128u};
    for(usize i = 0; i < 5; ++i) {
        const auto push = pushOf(first.dispatches[i].push);
        EXPECT_EQ(push.step, steps[i]) << i;
        EXPECT_EQ(first.dispatches[i].threads.x, threads[i]) << i;
        EXPECT_EQ(std::bit_cast<u32>(push.dt), std::bit_cast<u32>(EffectStep));
        EXPECT_EQ(push.seed, 5u);
    }
    EXPECT_EQ(
        pushOf(first.dispatches[0].push).particlesRW,
        FakeDevice::FirstWritableBufferID
    );
    EXPECT_EQ(first.dispatchBarriers.size(), 3u);
    EXPECT_EQ(first.bufferReleases.size(), 2u);

    f.Frame();
    EXPECT_TRUE(f.cmdList.violations.empty());
    const auto& second = f.cmdList.computePasses.at(0);
    ASSERT_EQ(second.dispatches.size(), 2u);
    EXPECT_EQ(pushOf(second.dispatches[0].push).step, 4u);
    EXPECT_EQ(pushOf(second.dispatches[1].push).step, 1u);
    EXPECT_TRUE(second.dispatchBarriers.empty());
    for(const auto& acquire: second.bufferAcquires)
        EXPECT_TRUE(acquire.crossSubmission);
    EXPECT_EQ(f.effects.Steps("a"), 5u);
    EXPECT_EQ(f.effects.Steps("b"), 2u);
}

// each draw is a strip per particle, depth-tested without writing, blended
// as its desc says, with the step it shows
TEST(EffectSystem, DrawsAStripPerParticle) {
    Fixture f;
    auto twoDraws = desc("glints", 64);
    twoDraws.draws.push_back({.entry = "halo", .blend = EffectBlend::Alpha});
    f.effects.Add(twoDraws);

    EXPECT_EQ(f.Frame(), 2u);
    EXPECT_TRUE(f.cmdList.violations.empty());
    const auto& pass = f.cmdList.passes.at(0);
    EXPECT_EQ(pass.directDraws, 2u);
    ASSERT_EQ(pass.pushes.size(), 2u);
    const auto push = pushOf(pass.pushes[0]);
    EXPECT_EQ(push.particles, FakeBuffer::ReadableID);
    EXPECT_EQ(push.step, 0u);
    EXPECT_EQ(push.count, 64u);

    ASSERT_EQ(f.device.pipelineCreates.size(), 2u);
    const auto& glow = f.device.pipelineCreates[0];
    const auto& frontend = std::get<RHILegacyFrontendDesc>(glow.preRasterizer);
    EXPECT_EQ(frontend.topology, RHIPrimitiveTopology::TriangleStrip);
    EXPECT_EQ(frontend.vertexShader.entryPoint, "vs_glow");
    EXPECT_EQ(glow.fragmentShader->entryPoint, "fs_glow");
    EXPECT_EQ(glow.rasterizer.cullMode, RHICullMode::None);
    ASSERT_TRUE(glow.depthStencil.has_value());
    EXPECT_FALSE(glow.depthStencil->depthWriteEnable);
    EXPECT_EQ(glow.depthStencil->depthFunc, RHIComparisonFunc::Less);
    EXPECT_EQ(glow.renderTargetCount, 1u);
    EXPECT_EQ(glow.renderTargetFormats[0], RHIPixelFormat::RGBA16_FLOAT);
    const auto& additive = glow.blend->renderTargets[0];
    EXPECT_TRUE(additive.blendEnable);
    EXPECT_EQ(additive.srcBlend, RHIBlend::One);
    EXPECT_EQ(additive.dstBlend, RHIBlend::One);
    const auto& alpha = f.device.pipelineCreates[1].blend->renderTargets[0];
    EXPECT_EQ(alpha.srcBlend, RHIBlend::SrcAlpha);
    EXPECT_EQ(alpha.dstBlend, RHIBlend::InvSrcAlpha);

    // a second frame resolves the same two pipelines
    f.Frame();
    EXPECT_EQ(f.device.pipelineCreates.size(), 2u);
}

// the prewarm counts back from the frame's world step, and the draws carry
// it, paused or not
TEST(EffectSystem, PushesCarryTheWorldStep) {
    Fixture f;
    f.effects.Add(desc("a", 32, 3));
    f.effects.Add(desc("b", 32));

    f.Frame();
    const auto& first = f.cmdList.computePasses.at(0);
    ASSERT_EQ(first.dispatches.size(), 5u);
    const std::array worldSteps{0u - 2u, 0u - 1u, 0u, 1u, 1u};
    for(usize i = 0; i < 5; ++i) {
        const auto push = pushOf(first.dispatches[i].push);
        EXPECT_EQ(push.worldStep, worldSteps[i]) << i;
    }

    f.effects.SetPaused(true);
    f.Frame();
    EXPECT_TRUE(f.cmdList.computePasses.empty());
    const auto& pass = f.cmdList.passes.at(0);
    ASSERT_EQ(pass.pushes.size(), 2u);
    for(const auto& push: pass.pushes)
        EXPECT_EQ(pushOf(push).worldStep, 2u);
}

// a frame takes the steps its clock owes: two up to the world step, or none
// while the draws still show it
TEST(EffectSystem, AFrameTakesTheStepsItsClockOwes) {
    Fixture f;
    f.effects.Add(desc("a", 32));
    f.Frame();

    f.Frame(2);
    const auto& pass = f.cmdList.computePasses.at(0);
    ASSERT_EQ(pass.dispatches.size(), 2u);
    EXPECT_EQ(pushOf(pass.dispatches[0].push).worldStep, 2u);
    EXPECT_EQ(pushOf(pass.dispatches[1].push).worldStep, 3u);
    EXPECT_EQ(f.effects.Steps("a"), 3u);

    f.Frame(0);
    EXPECT_TRUE(f.cmdList.computePasses.empty());
    EXPECT_TRUE(f.cmdList.violations.empty());
    EXPECT_EQ(pushOf(f.cmdList.passes.at(0).pushes.at(0)).worldStep, 3u);
    EXPECT_EQ(f.effects.Steps("a"), 3u);
}

// paused, a started system records no step and no acquire for the draws
TEST(EffectSystem, APausedSystemStepsNothingOnceStarted) {
    Fixture f;
    f.effects.Add(desc("a", 32));
    f.effects.SetPaused(true);

    f.Frame();
    EXPECT_EQ(f.cmdList.computePasses.size(), 1u);
    f.Frame();
    EXPECT_TRUE(f.cmdList.computePasses.empty());
    EXPECT_TRUE(f.cmdList.passes.at(0).bufferAcquires.empty());
    EXPECT_TRUE(f.cmdList.violations.empty());
    EXPECT_EQ(f.effects.Steps("a"), 1u);
}

// the basis FlyCamera's rotation gives its right and up
TEST(EffectView, IsTheCameraRotationsRightAndUp) {
    const auto expectNear = [](Vec3 actual, Vec3 expected) {
        EXPECT_NEAR(actual.x, expected.x, 1e-5f);
        EXPECT_NEAR(actual.y, expected.y, 1e-5f);
        EXPECT_NEAR(actual.z, expected.z, 1e-5f);
    };
    const auto level = effectViewOf(
        viewMat(Vec3{1.0f, 2.0f, 3.0f}, quat(rotateY(0.0f), rotateX(0.0f)))
    );
    expectNear(level.right, unitX());
    expectNear(level.up, unitY());

    const auto rotation = quat(rotateY(0.7f), rotateX(0.3f));
    const auto basis = rotateMat(rotation);
    const auto turned =
        effectViewOf(viewMat(Vec3{-4.0f, 1.0f, 2.0f}, rotation));
    expectNear(turned.right, static_cast<Vec3>(basis[0]));
    expectNear(turned.up, static_cast<Vec3>(basis[1]));
}

// a reload swaps every effect's step for a fresh one, or none when one fails
TEST(EffectSystem, AReloadSwapsEveryKernelOrNone) {
    Fixture f;
    f.effects.Add(desc("a", 32));
    f.effects.Add(desc("b", 32));
    f.Frame();
    const auto* before =
        f.cmdList.computePasses.at(0).dispatches.at(0).pipeline;

    // the clear, two steps and the draws' pipeline were creates 1 to 4; the
    // second rebuild, create 6, fails and the first is thrown away unused
    f.device.failAt = 6;
    EXPECT_THROW(f.effects.ReloadKernels(), std::runtime_error);
    EXPECT_TRUE(f.device.deferred.empty());
    EXPECT_EQ(f.device.computeDestroyed, 1u);
    f.Frame();
    EXPECT_EQ(f.cmdList.computePasses.at(0).dispatches.at(0).pipeline, before);

    f.device.failAt = 0;
    EXPECT_EQ(f.effects.ReloadKernels(), 2u);
    EXPECT_EQ(f.device.deferred.size(), 2u);
    EXPECT_EQ(f.device.computeDestroyed, 1u);
    f.Frame();
    EXPECT_NE(f.cmdList.computePasses.at(0).dispatches.at(0).pipeline, before);
    f.device.RunDeferred();
    EXPECT_EQ(f.device.computeDestroyed, 3u);
}

// an effect added while paused runs its first frame, and the paused ones
// stay where they are
TEST(EffectSystem, AnEffectAddedWhilePausedStartsAlone) {
    Fixture f;
    f.effects.Add(desc("a", 32));
    f.Frame();
    f.effects.SetPaused(true);
    f.effects.Add(desc("b", 32, 2));

    f.Frame();
    EXPECT_TRUE(f.cmdList.violations.empty());
    EXPECT_EQ(f.cmdList.computePasses.at(0).dispatches.size(), 3u);
    EXPECT_EQ(f.effects.Steps("a"), 1u);
    EXPECT_EQ(f.effects.Steps("b"), 3u);
}
