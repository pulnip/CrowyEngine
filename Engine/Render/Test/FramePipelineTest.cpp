#include <array>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "EnumUtil.hpp"
#include "FakeCommandList.hpp"
#include "FakeDevice.hpp"
#include "FramePipeline.hpp"
#include "LinearAlgebra.hpp"
#include "RHICommandList.hpp"
#include "RenderScene.hpp"
#include "SceneRenderer.hpp"
#include "StandardPipeline.hpp"

using namespace Crowy;

namespace
{
    using Objects = std::vector<u32>;
    using Pushes = std::vector<ScenePush>;
    using RecordedPass = FakeCommandList::RecordedPass;
    using TextureBarriers = std::vector<RHITextureBarrier>;

    constexpr auto BackBufferFormat = RHIPixelFormat::RGBA8_UNORM;
    constexpr u32 Width = 64;
    constexpr u32 Height = 32;
    constexpr u64 Vertices = 0xFE;

    // a fragment entry per pipeline, so two entries key two pipelines
    MaterialPipelineDesc Pipeline(
        CStr fragmentEntry,
        MaterialDomain domain = MaterialDomain::Opaque
    ) {
        return MaterialPipelineDesc{
            .vertexShader =
                {.path = "Engine/Shader/X.slang", .entryPoint = "vs_main"},
            .fragmentShader =
                {.path = "Engine/Shader/X.slang", .entryPoint = fragmentEntry},
            .domain = domain,
            .profile = "sm_6_8"
        };
    }

    MeshPassDesc& Mesh(PassDesc& pass) {
        return std::get<MeshPassDesc>(pass.kind);
    }

    const MeshPassDesc& Mesh(const PassDesc& pass) {
        return std::get<MeshPassDesc>(pass.kind);
    }

    // the standard list behind a fixed-size depth-only pass on view 1,
    // drawing casters only: the shape a shadow pass takes
    FramePipelineDesc ShadowShaped(const PassDepthBias& bias) {
        auto desc = makeStandardPipeline({});
        desc.targets.push_back(
            FrameTargetDesc{
                .name = "ShadowMap",
                .format = RHIPixelFormat::D32_FLOAT,
                .width = 64,
                .height = 64
            }
        );
        const auto shadowMap = static_cast<FrameTargetID>(desc.targets.size());
        desc.passes.insert(
            desc.passes.begin(),
            PassDesc{
                .name = "Shadow",
                .depth = DepthTargetUse{.target = shadowMap},
                .kind = MeshPassDesc{
                    .view = 1,
                    .filter =
                        DrawFilter{
                            .domains = MaterialDomain::Opaque,
                            .required = PrimitiveFlags::CastShadow
                        },
                    .state = MeshPassState{.depthBias = bias}
                }
            }
        );

        return desc;
    }

    // a scene whose primitives all sit inside both views' clip space
    class Fixture {
    public:
        FakeDevice device;
        RenderScene scene;
        SceneRenderer renderer{device, SceneRendererDesc{.viewCount = 2}};
        FakeCommandList cmdList;
        FakeTexture backBuffer{BackBufferFormat, Width, Height, 0xBB};
        FakeBuffer indexBuffer{64};

        MaterialHandle AddMaterial(
            CStr fragmentEntry,
            MaterialDomain domain = MaterialDomain::Opaque
        ) {
            return scene.Materials().Add(
                MaterialResource{.pipeline = Pipeline(fragmentEntry, domain)}
            );
        }

        // returns the primitive's row
        u32 AddPrimitive(
            MaterialHandle material,
            PrimitiveFlags flags =
                combine(PrimitiveFlags::Visible, PrimitiveFlags::CastShadow)
        ) {
            const auto mesh = scene.Meshes().Add(
                MeshResource{
                    .subMeshes = {SubMesh{.geometry = {.indexCount = 3}}},
                    .materials = {material}
                }
            );
            scene.Primitives().Add(
                PrimitiveSnapshot{
                    .worldBounds =
                        AABB3D{
                            .center = {0.0f, 0.0f, 0.5f},
                            .halfScale = 0.1f * ones()
                        },
                    .mesh = mesh,
                    .flags = flags
                }
            );

            return static_cast<u32>(scene.Primitives().Count() - 1);
        }

        FrameInputs Inputs() {
            return FrameInputs{
                .backBuffer = &backBuffer,
                .indices = RHIIndexBufferView{.buffer = &indexBuffer},
                .vertices = Vertices
            };
        }

        void Prepare(
            FramePipeline& pipeline,
            const MeshPassOverride& debug = {}
        ) {
            renderer.BeginFrame(scene);
            pipeline.Prepare(renderer, scene, debug);
            renderer.Upload();
        }

        void Frame(FramePipeline& pipeline, FrameInputs& inputs) {
            Prepare(pipeline);
            cmdList.Begin();
            pipeline.Record(cmdList, renderer, inputs);
            cmdList.Close();
        }

        const RecordedPass& Pass(StrView name) const {
            for(const auto& pass: cmdList.passes) {
                if(pass.event == name)
                    return pass;
            }
            ADD_FAILURE() << "no pass named " << name;
            static const RecordedPass none;

            return none;
        }

        // the rows a pass's draws read, as its push names them
        Objects DrawnObjects(const RecordedPass& pass) const {
            if(pass.pushes.empty())
                return {};

            ScenePush push;
            std::memcpy(&push, pass.pushes.back().data(), sizeof(push));
            u32 draws = 0;
            for(const auto& batch: pass.batches)
                draws += batch.drawCount;

            Objects objects;
            for(u32 i = 0; i < draws; ++i) {
                objects.push_back(device.transient
                                      .Read<DrawData>(
                                          (push.drawBase + i) *
                                          static_cast<u32>(sizeof(DrawData))
                                      )
                                      .objectID);
            }

            return objects;
        }
    };

    void ExpectRefused(FramePipelineDesc desc, StrView expected) {
        FakeDevice device;
        try {
            FramePipeline pipeline(
                device,
                std::move(desc),
                BackBufferFormat,
                Width,
                Height
            );
            ADD_FAILURE() << "accepted a desc that should say: " << expected;
        } catch(const std::invalid_argument& e) {
            EXPECT_TRUE(StrView{e.what()}.contains(expected))
                << e.what() << "\nexpected: " << expected;
        }
    }
}

TEST(FramePipeline, StandardPipelineIsPrepassOpaqueTranslucent) {
    const auto desc = makeStandardPipeline({});

    ASSERT_EQ(desc.targets.size(), 1u);
    EXPECT_EQ(desc.targets[0].name, "SceneDepth");
    EXPECT_EQ(desc.targets[0].format, RHIPixelFormat::D32_FLOAT);
    EXPECT_EQ(desc.targets[0].width, 0u);
    EXPECT_EQ(desc.sceneColor, BackBufferTarget);
    ASSERT_EQ(desc.passes.size(), 3u);

    const auto& prepass = desc.passes[0];
    EXPECT_EQ(prepass.name, "DepthPrepass");
    EXPECT_TRUE(prepass.colors.empty());
    ASSERT_TRUE(prepass.depth.has_value());
    EXPECT_EQ(prepass.depth->load, RHILoadAction::Clear);
    EXPECT_EQ(prepass.depth->store, RHIStoreAction::Store);
    EXPECT_EQ(Mesh(prepass).filter.domains, MaterialDomain::Opaque);
    EXPECT_EQ(Mesh(prepass).state.depthFunc, RHIComparisonFunc::Less);
    EXPECT_TRUE(Mesh(prepass).state.depthWrite);

    const auto& opaque = desc.passes[1];
    EXPECT_EQ(opaque.name, "Opaque");
    ASSERT_EQ(opaque.colors.size(), 1u);
    EXPECT_EQ(opaque.colors[0].target, BackBufferTarget);
    EXPECT_EQ(opaque.colors[0].load, RHILoadAction::Clear);
    ASSERT_TRUE(opaque.depth.has_value());
    EXPECT_EQ(opaque.depth->load, RHILoadAction::Load);
    EXPECT_EQ(opaque.depth->store, RHIStoreAction::Store);
    EXPECT_EQ(Mesh(opaque).order, DrawOrder::PipelineThenNearFirst);
    EXPECT_EQ(Mesh(opaque).state.depthFunc, RHIComparisonFunc::Equal);
    EXPECT_FALSE(Mesh(opaque).state.depthWrite);

    const auto& translucent = desc.passes[2];
    EXPECT_EQ(translucent.name, "Translucent");
    ASSERT_EQ(translucent.colors.size(), 1u);
    EXPECT_EQ(translucent.colors[0].load, RHILoadAction::Load);
    ASSERT_TRUE(translucent.depth.has_value());
    EXPECT_EQ(translucent.depth->load, RHILoadAction::Load);
    EXPECT_EQ(translucent.depth->store, RHIStoreAction::DontCare);
    EXPECT_EQ(Mesh(translucent).filter.domains, MaterialDomain::Translucent);
    EXPECT_EQ(Mesh(translucent).order, DrawOrder::FarFirst);
    EXPECT_EQ(Mesh(translucent).state.depthFunc, RHIComparisonFunc::Less);
    EXPECT_FALSE(Mesh(translucent).state.depthWrite);
}

TEST(FramePipeline, StandardPipelineWithoutPrepassWritesDepthInOpaque) {
    const auto desc = makeStandardPipeline({.depthPrepass = false});

    ASSERT_EQ(desc.passes.size(), 2u);
    const auto& opaque = desc.passes[0];
    EXPECT_EQ(opaque.name, "Opaque");
    ASSERT_TRUE(opaque.depth.has_value());
    EXPECT_EQ(opaque.depth->load, RHILoadAction::Clear);
    EXPECT_EQ(opaque.depth->store, RHIStoreAction::Store);
    EXPECT_EQ(Mesh(opaque).state.depthFunc, RHIComparisonFunc::Less);
    EXPECT_TRUE(Mesh(opaque).state.depthWrite);
    EXPECT_EQ(desc.passes[1].name, "Translucent");
}

// every half as the edge rule gives it, two frames running
TEST(FramePipeline, WalkRecordsEveryEdgeOfTheStandardPipeline) {
    using enum RHIResourceUsage;

    Fixture f;
    f.AddPrimitive(f.AddMaterial("fs_opaque"));
    f.AddPrimitive(f.AddMaterial("fs_glass", MaterialDomain::Translucent));
    FramePipeline pipeline(
        f.device,
        makeStandardPipeline({}),
        BackBufferFormat,
        Width,
        Height
    );
    auto inputs = f.Inputs();

    for(int frame = 0; frame < 2; ++frame) {
        f.Frame(pipeline, inputs);

        ASSERT_EQ(f.cmdList.passes.size(), 3u);
        auto& depth = *f.Pass("DepthPrepass").depth->texture;
        auto& back = f.backBuffer;

        EXPECT_EQ(
            f.Pass("DepthPrepass").acquires,
            TextureBarriers{
                MakeCrossSubmissionBarrier(depth, DepthWrite, DepthWrite, true)
            }
        );
        EXPECT_EQ(
            f.Pass("DepthPrepass").releases,
            TextureBarriers{MakeBarrier(depth, DepthWrite, DepthWrite)}
        );
        EXPECT_EQ(
            f.Pass("Opaque").acquires,
            (TextureBarriers{
                MakeBarrier(back, Undefined, RenderTarget),
                MakeBarrier(depth, DepthWrite, DepthWrite)
            })
        );
        EXPECT_EQ(
            f.Pass("Opaque").releases,
            (TextureBarriers{
                MakeBarrier(back, RenderTarget, RenderTarget),
                MakeBarrier(depth, DepthWrite, DepthWrite)
            })
        );
        EXPECT_EQ(
            f.Pass("Translucent").acquires,
            (TextureBarriers{
                MakeBarrier(back, RenderTarget, RenderTarget),
                MakeBarrier(depth, DepthWrite, DepthWrite)
            })
        );
        EXPECT_EQ(
            f.Pass("Translucent").releases,
            TextureBarriers{MakeBarrier(back, RenderTarget, Present)}
        );

        EXPECT_TRUE(f.cmdList.violations.empty())
            << f.cmdList.violations.front().what;
        EXPECT_TRUE(f.cmdList.unconsumedAtClose.empty());
#if CROWY_FRAME_STATS
        EXPECT_EQ(f.cmdList.GetStats().renderPassCount, 3u);
        EXPECT_EQ(f.cmdList.GetStats().barrierEdgeCount, 9u);
#endif
    }
}

TEST(FramePipeline, BackBufferIsAcquiredUndefinedAndReleasedToPresent) {
    Fixture f;
    f.AddPrimitive(f.AddMaterial("fs_opaque"));
    FramePipeline pipeline(
        f.device,
        makeStandardPipeline({}),
        BackBufferFormat,
        Width,
        Height
    );
    auto inputs = f.Inputs();
    inputs.sceneClear = Color{0.25f, 0.5f, 0.75f, 1.0f};

    f.Frame(pipeline, inputs);

    const auto& opaque = f.Pass("Opaque");
    ASSERT_EQ(opaque.colors.size(), 1u);
    EXPECT_EQ(opaque.colors[0].texture, &f.backBuffer);
    EXPECT_EQ(opaque.colors[0].loadAction, RHILoadAction::Clear);
    EXPECT_EQ(opaque.colors[0].clearColor, inputs.sceneClear);
    ASSERT_FALSE(opaque.acquires.empty());
    EXPECT_TRUE(opaque.acquires.front().discard);
    EXPECT_EQ(
        opaque.acquires.front().layoutBefore,
        RHITextureLayout::Undefined
    );
    EXPECT_EQ(f.Pass("Translucent").colors[0].loadAction, RHILoadAction::Load);
    EXPECT_EQ(f.cmdList.LayoutOf(&f.backBuffer), RHITextureLayout::Present);
    EXPECT_TRUE(f.cmdList.violations.empty());
}

// a fresh texture, the frame before's last use, and a texture a Resize made
TEST(FramePipeline, FirstUseAcquiresAcrossSubmissionsAndDiscards) {
    Fixture f;
    f.AddPrimitive(f.AddMaterial("fs_opaque"));
    FramePipeline pipeline(
        f.device,
        makeStandardPipeline({}),
        BackBufferFormat,
        Width,
        Height
    );
    auto inputs = f.Inputs();

    const auto expectFirstUse = [&f] {
        const auto& acquire = f.Pass("DepthPrepass").acquires.front();
        EXPECT_TRUE(acquire.crossSubmission);
        EXPECT_TRUE(acquire.discard);
        EXPECT_EQ(acquire.layoutBefore, RHITextureLayout::Undefined);
        EXPECT_EQ(acquire.syncBefore, RHIBarrierSync::DepthStencil);
    };

    f.Frame(pipeline, inputs);
    expectFirstUse();
    auto* first = f.Pass("DepthPrepass").depth->texture;

    f.Frame(pipeline, inputs);
    expectFirstUse();
    EXPECT_EQ(f.Pass("DepthPrepass").depth->texture, first);
    EXPECT_EQ(f.cmdList.LayoutOf(first), RHITextureLayout::DepthWrite);

    pipeline.Resize(128, 64);
    f.Frame(pipeline, inputs);
    expectFirstUse();
    auto* resized = f.Pass("DepthPrepass").depth->texture;
    EXPECT_NE(resized, first);
    EXPECT_EQ(resized->GetWidth(), 128u);
    EXPECT_EQ(resized->GetHeight(), 64u);

    EXPECT_TRUE(f.cmdList.violations.empty());
}

TEST(FramePipeline, EveryMeshPassRebindsItsViewAndPush) {
    Fixture f;
    const auto opaque = f.AddPrimitive(f.AddMaterial("fs_opaque"));
    const auto glass =
        f.AddPrimitive(f.AddMaterial("fs_glass", MaterialDomain::Translucent));
    FramePipeline pipeline(
        f.device,
        makeStandardPipeline({}),
        BackBufferFormat,
        Width,
        Height
    );
    Pushes pushes;
    auto inputs = f.Inputs();
    inputs.bindMeshPass =
        [&pushes](RHICommandList& cmdList, const ScenePush& push) {
            pushes.push_back(push);
            cmdList.SetPushGraphicsConstants(push);
        };

    f.Frame(pipeline, inputs);

    ASSERT_EQ(pushes.size(), 3u);
    for(const auto& push: pushes)
        EXPECT_EQ(push.vertices, Vertices);

    const auto viewOffset =
        f.Pass("DepthPrepass").constantBuffers.front().offset;
    for(const auto& pass: f.cmdList.passes) {
        ASSERT_EQ(pass.constantBuffers.size(), 1u) << pass.event;
        EXPECT_EQ(pass.constantBuffers[0].slot, ViewConstantBufferSlot);
        EXPECT_EQ(pass.constantBuffers[0].offset, viewOffset);
        EXPECT_EQ(pass.pushes.size(), 1u) << pass.event;
    }

    // each pass's push names its own list's rows
    EXPECT_EQ(f.DrawnObjects(f.Pass("DepthPrepass")), Objects{opaque});
    EXPECT_EQ(f.DrawnObjects(f.Pass("Opaque")), Objects{opaque});
    EXPECT_EQ(f.DrawnObjects(f.Pass("Translucent")), Objects{glass});
    EXPECT_NE(pushes[1].drawBase, pushes[2].drawBase);
    EXPECT_TRUE(f.cmdList.violations.empty());
}

TEST(FramePipeline, GeometryAcquiresRideEveryMeshPass) {
    Fixture f;
    f.AddPrimitive(f.AddMaterial("fs_opaque"));
    FramePipeline pipeline(
        f.device,
        makeStandardPipeline({}),
        BackBufferFormat,
        Width,
        Height
    );
    FakeBuffer vertexBuffer{64};
    const std::array uploads{
        MakeBarrier(
            vertexBuffer,
            RHIResourceUsage::CopyDst,
            RHIResourceUsage::VertexBuffer
        ),
        MakeBarrier(
            f.indexBuffer,
            RHIResourceUsage::CopyDst,
            RHIResourceUsage::IndexBuffer
        )
    };
    auto inputs = f.Inputs();
    inputs.geometryAcquires = uploads;

    f.Prepare(pipeline);
    f.cmdList.Begin();
    // the pool's copies, recorded outside any pass
    f.cmdList.BeginBlitPass();
    f.cmdList.EndBlitPass({}, uploads);
    pipeline.Record(f.cmdList, f.renderer, inputs);
    f.cmdList.Close();

    for(const auto& pass: f.cmdList.passes) {
        EXPECT_EQ(
            pass.bufferAcquires,
            (std::vector<RHIBufferBarrier>{uploads.begin(), uploads.end()})
        ) << pass.event;
    }
    EXPECT_TRUE(f.cmdList.violations.empty());
    EXPECT_TRUE(f.cmdList.unconsumedBuffersAtClose.empty());

    const auto stats = pipeline.Stats();
    ASSERT_EQ(stats.size(), 3u);
    EXPECT_EQ(stats[0].barrierEdges, 2u + 2u);
    EXPECT_EQ(stats[1].barrierEdges, 4u + 2u);
    EXPECT_EQ(stats[2].barrierEdges, 3u + 2u);
}

TEST(FramePipeline, OverlayRidesTheLastBackBufferPass) {
    Fixture f;
    f.AddPrimitive(f.AddMaterial("fs_opaque"));
    f.AddPrimitive(f.AddMaterial("fs_glass", MaterialDomain::Translucent));
    FramePipeline pipeline(
        f.device,
        makeStandardPipeline({}),
        BackBufferFormat,
        Width,
        Height
    );
    FakeTexture atlas{RHIPixelFormat::RGBA8_UNORM, 8, 8, 0xA7};
    const std::array uiAcquires{MakeBarrier(
        atlas,
        RHIResourceUsage::Undefined,
        RHIResourceUsage::SampledFragment
    )};
    auto inputs = f.Inputs();
    inputs.overlayAcquires = uiAcquires;
    inputs.recordOverlay = [](RHICommandList& cmdList) {
        cmdList.SetMarker("ui");
    };

    f.Frame(pipeline, inputs);

    EXPECT_EQ(
        pipeline.Overlay(),
        (OverlayFormats{
            .color = BackBufferFormat,
            .depth = RHIPixelFormat::D32_FLOAT
        })
    );
    for(const auto& pass: f.cmdList.passes) {
        const bool carries =
            std::ranges::contains(pass.acquires, uiAcquires[0]);
        const bool marked = std::ranges::contains(pass.log, Str{"marker ui"});
        EXPECT_EQ(carries, pass.event == "Translucent") << pass.event;
        EXPECT_EQ(marked, pass.event == "Translucent") << pass.event;
    }

    // after the pass's own draws
    const auto& log = f.Pass("Translucent").log;
    ASSERT_GE(log.size(), 2u);
    EXPECT_EQ(log.back(), "marker ui");
    EXPECT_EQ(log[log.size() - 2], "draw");
    EXPECT_TRUE(f.cmdList.violations.empty());
}

TEST(FramePipeline, ResizeRecreatesOnlySwapchainSizedTargets) {
    Fixture f;
    FramePipeline pipeline(
        f.device,
        ShadowShaped(PassDepthBias{}),
        BackBufferFormat,
        Width,
        Height
    );

    ASSERT_EQ(f.device.textureCreates.size(), 2u);
    EXPECT_EQ(f.device.textureCreates[0].width, Width);
    EXPECT_EQ(f.device.textureCreates[0].height, Height);
    EXPECT_EQ(f.device.textureCreates[0].usage, RHITextureUsage::DepthStencil);
    EXPECT_EQ(f.device.textureCreates[1].width, 64u);
    EXPECT_EQ(f.device.textureCreates[1].height, 64u);

    pipeline.Resize(128, 96);

    ASSERT_EQ(f.device.textureCreates.size(), 3u);
    EXPECT_EQ(f.device.textureCreates[2].width, 128u);
    EXPECT_EQ(f.device.textureCreates[2].height, 96u);
    EXPECT_EQ(f.device.textureCreates[2].format, RHIPixelFormat::D32_FLOAT);
    // a frame in flight may still read the old one
    EXPECT_EQ(f.device.deferred.size(), 1u);
    EXPECT_EQ(f.device.texturesDestroyed, 0u);

    f.device.RunDeferred();
    EXPECT_EQ(f.device.texturesDestroyed, 1u);
}

TEST(FramePipeline, AShadowShapedPassDrawsCastersFromViewOne) {
    Fixture f;
    const auto material = f.AddMaterial("fs_opaque");
    const auto caster = f.AddPrimitive(material);
    const auto receiver = f.AddPrimitive(material, PrimitiveFlags::Visible);
    const PassDepthBias bias{.depthBias = 2, .slopeScaledDepthBias = 1.5f};
    FramePipeline
        pipeline(f.device, ShadowShaped(bias), BackBufferFormat, Width, Height);
    auto inputs = f.Inputs();

    f.Frame(pipeline, inputs);

    const auto& shadow = f.Pass("Shadow");
    EXPECT_TRUE(shadow.colors.empty());
    ASSERT_TRUE(shadow.viewport.has_value());
    EXPECT_EQ(shadow.viewport->width, 64.0f);
    EXPECT_EQ(shadow.viewport->height, 64.0f);
    ASSERT_EQ(shadow.constantBuffers.size(), 1u);
    EXPECT_EQ(
        shadow.constantBuffers[0].offset,
        f.Pass("Opaque").constantBuffers[0].offset + sizeof(ViewData)
    );
    EXPECT_EQ(f.DrawnObjects(shadow), Objects{caster});
    EXPECT_EQ(f.DrawnObjects(f.Pass("Opaque")), (Objects{caster, receiver}));

    // the bias is in the key: the prepass's depth-only pipeline is another one
    ASSERT_EQ(shadow.batches.size(), 1u);
    auto* biased = &f.renderer.Pipelines().Resolve(
        f.scene.Materials().GetRef(material).pipeline,
        PassPipelineDesc{.state = MeshPassState{.depthBias = bias}}
    );
    EXPECT_EQ(shadow.batches[0].pso, biased);
    EXPECT_NE(shadow.batches[0].pso, f.Pass("DepthPrepass").batches[0].pso);

    auto* map = shadow.depth->texture;
    pipeline.Resize(128, 96);
    f.Frame(pipeline, inputs);
    EXPECT_EQ(f.Pass("Shadow").depth->texture, map);
    EXPECT_EQ(map->GetWidth(), 64u);
    EXPECT_TRUE(f.cmdList.violations.empty());
}

TEST(FramePipeline, APassNotInTheListCostsNothing) {
    const auto frameWith = [](bool depthPrepass) {
        Fixture f;
        f.AddPrimitive(f.AddMaterial("fs_opaque"));
        f.AddPrimitive(f.AddMaterial("fs_glass", MaterialDomain::Translucent));
        FramePipeline pipeline(
            f.device,
            makeStandardPipeline({.depthPrepass = depthPrepass}),
            BackBufferFormat,
            Width,
            Height
        );
        auto inputs = f.Inputs();
        f.Frame(pipeline, inputs);

        EXPECT_TRUE(f.cmdList.violations.empty());
        EXPECT_EQ(f.device.textureCreates.size(), 1u);

        return std::pair{f.cmdList.passes.size(), f.renderer.PipelineCount()};
    };

    // a depth-only key beside the two colour keys
    EXPECT_EQ(frameWith(true), (std::pair<usize, usize>{3, 3}));
    EXPECT_EQ(frameWith(false), (std::pair<usize, usize>{2, 2}));
}

TEST(FramePipeline, DestroyedPipelineRetiresItsTargets) {
    Fixture f;
    f.AddPrimitive(f.AddMaterial("fs_opaque"));
    {
        FramePipeline pipeline(
            f.device,
            ShadowShaped(PassDepthBias{}),
            BackBufferFormat,
            Width,
            Height
        );
        auto inputs = f.Inputs();
        f.Frame(pipeline, inputs);
    }

    EXPECT_EQ(f.device.deferred.size(), 2u);
    EXPECT_EQ(f.device.texturesDestroyed, 0u);
    f.device.RunDeferred();
    EXPECT_EQ(f.device.texturesDestroyed, 2u);
}

TEST(FramePipeline, PassStatsCountEachPass) {
    Fixture f;
    f.AddPrimitive(f.AddMaterial("fs_a"));
    f.AddPrimitive(f.AddMaterial("fs_b"));
    f.AddPrimitive(f.AddMaterial("fs_glass", MaterialDomain::Translucent));
    FramePipeline pipeline(
        f.device,
        makeStandardPipeline({}),
        BackBufferFormat,
        Width,
        Height
    );
    auto inputs = f.Inputs();

    f.Frame(pipeline, inputs);

    const auto stats = pipeline.Stats();
    ASSERT_EQ(stats.size(), 3u);
    // the two opaque materials share one depth-only pipeline
    EXPECT_EQ(stats[0].name, "DepthPrepass");
    EXPECT_EQ(stats[0].draws, 2u);
    EXPECT_EQ(stats[0].runs, 1u);
    EXPECT_EQ(stats[0].triangles, 2u);
    EXPECT_EQ(stats[0].barrierEdges, 2u);
    EXPECT_EQ(stats[1].name, "Opaque");
    EXPECT_EQ(stats[1].draws, 2u);
    EXPECT_EQ(stats[1].runs, 2u);
    EXPECT_EQ(stats[1].triangles, 2u);
    EXPECT_EQ(stats[1].barrierEdges, 4u);
    EXPECT_EQ(stats[2].name, "Translucent");
    EXPECT_EQ(stats[2].draws, 1u);
    EXPECT_EQ(stats[2].runs, 1u);
    EXPECT_EQ(stats[2].triangles, 1u);
    EXPECT_EQ(stats[2].barrierEdges, 3u);
}

TEST(FramePipeline, InvalidDescsAreRefused) {
    const auto standard = [] {
        return makeStandardPipeline({});
    };
    constexpr FrameTargetID SceneDepth = 1;

    {
        auto desc = standard();
        desc.sceneColor = 5;
        ExpectRefused(std::move(desc), "sceneColor names target 5");
    }
    {
        auto desc = standard();
        desc.passes[1].colors[0].target = 9;
        ExpectRefused(std::move(desc), "pass 'Opaque': target 9 is unknown");
    }
    {
        auto desc = standard();
        desc.targets.push_back(FrameTargetDesc{.name = "SceneDepth"});
        ExpectRefused(std::move(desc), "two targets are named 'SceneDepth'");
    }
    {
        auto desc = standard();
        desc.passes[2].name = "Opaque";
        ExpectRefused(
            std::move(desc),
            "pass 'Opaque': two passes have this name"
        );
    }
    {
        auto desc = standard();
        desc.passes[1].colors.push_back(
            ColorTargetUse{.target = BackBufferTarget}
        );
        ExpectRefused(
            std::move(desc),
            "pass 'Opaque': it attaches target 0 twice"
        );
    }
    {
        auto desc = standard();
        desc.passes[0].depth->target = BackBufferTarget;
        ExpectRefused(
            std::move(desc),
            "pass 'DepthPrepass': the back buffer is not a depth target"
        );
    }
    {
        auto desc = standard();
        desc.passes[0].depth->load = RHILoadAction::Load;
        ExpectRefused(
            std::move(desc),
            "pass 'DepthPrepass': its first use of 'SceneDepth' keeps contents "
            "nothing wrote"
        );
    }
    {
        auto desc = standard();
        desc.passes[1].colors[0].load = RHILoadAction::Load;
        ExpectRefused(
            std::move(desc),
            "pass 'Opaque': its first use of the back buffer keeps contents"
        );
    }
    {
        auto desc = standard();
        desc.passes[1].depth->store = RHIStoreAction::DontCare;
        ExpectRefused(
            std::move(desc),
            "pass 'Translucent': it keeps 'SceneDepth', which pass 'Opaque' "
            "stored DontCare"
        );
    }
    {
        auto desc = standard();
        desc.passes.resize(1);
        ExpectRefused(std::move(desc), "no pass writes the back buffer");
    }
    {
        auto desc = standard();
        desc.passes[1].depth.reset();
        ExpectRefused(
            std::move(desc),
            "pass 'Opaque': a mesh pass needs a depth target"
        );
    }
    {
        auto desc = standard();
        Mesh(desc.passes[0]).state.fragmentShader = RHIShaderDesc{
            .path = "Engine/Shader/X.slang",
            .entryPoint = "fs_normals"
        };
        ExpectRefused(
            std::move(desc),
            "pass 'DepthPrepass': a depth-only mesh pass has no fragment stage"
        );
    }
    {
        auto desc = standard();
        desc.targets.push_back(
            FrameTargetDesc{
                .name = "Extra",
                .format = RHIPixelFormat::RGBA8_UNORM
            }
        );
        desc.passes[2].colors.push_back(
            ColorTargetUse{
                .target = static_cast<FrameTargetID>(desc.targets.size())
            }
        );
        ExpectRefused(
            std::move(desc),
            "pass 'Translucent': the overlay pass needs exactly one color "
            "target"
        );
    }
    {
        auto desc = standard();
        desc.passes[2].reads = {SceneDepth};
        ExpectRefused(
            std::move(desc),
            "pass 'Translucent': a pass that reads is not supported"
        );
    }
    {
        auto desc = standard();
        desc.passes[2].kind = FullscreenPassDesc{};
        ExpectRefused(
            std::move(desc),
            "pass 'Translucent': a fullscreen pass is not supported"
        );
    }
}
