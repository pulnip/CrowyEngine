#include <algorithm>
#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "EnumUtil.hpp"
#include "FakeCommandList.hpp"
#include "FakeDevice.hpp"
#include "FramePipeline.hpp"
#include "PostChain.hpp"
#include "RHICommandList.hpp"
#include "RenderScene.hpp"
#include "SceneRenderer.hpp"

// A second pipeline composed from Engine/Render/Public alone: the round's
// neutrality rule checked before any style exists.
using namespace Crowy;

namespace
{
    using RecordedPass = FakeCommandList::RecordedPass;
    using TextureBarriers = std::vector<RHITextureBarrier>;

    constexpr auto BackBufferFormat = RHIPixelFormat::RGBA8_UNORM;
    constexpr u32 Width = 64;
    constexpr u32 Height = 32;

    constexpr FrameTargetID SceneDepth = 1;
    constexpr FrameTargetID Normals = 2;
    constexpr FrameTargetID SceneColor = 3;

    const RHIShaderDesc normalsShader{
        .path = "Engine/Render/Sample/NormalsPrepass.slang",
        .entryPoint = "fs_normals"
    };
    const RHIShaderDesc outlineShader{
        .path = "Engine/Render/Sample/Outline.slang",
        .entryPoint = "fs_outline"
    };

    RHIBlendState OutlineBlend() {
        RHIBlendState blend{};
        blend.renderTargets[0] = RHIRenderTargetBlendState{
            .blendEnable = true,
            .srcBlend = RHIBlend::SrcAlpha,
            .dstBlend = RHIBlend::InvSrcAlpha
        };

        return blend;
    }

    PostPassDesc Post(CStr name, CStr entry) {
        return PostPassDesc{
            .name = name,
            .fragmentShader =
                {.path = "Engine/Render/Sample/Post.slang", .entryPoint = entry}
        };
    }

    // normals beside depth, an outline between the opaque and translucent
    // passes, then two post passes, the last carrying the UI
    FramePipelineDesc ToonShaped() {
        FramePipelineDesc desc{
            .targets =
                {FrameTargetDesc{
                     .name = "SceneDepth",
                     .format = RHIPixelFormat::D32_FLOAT
                 },
                 FrameTargetDesc{
                     .name = "Normals",
                     .format = RHIPixelFormat::RGBA8_UNORM,
                     .clearColor = {0.5f, 0.5f, 0.5f, 0.0f}
                 },
                 FrameTargetDesc{
                     .name = "SceneColor",
                     .format = RHIPixelFormat::RGBA16_FLOAT
                 }},
            .passes =
                {PassDesc{
                     .name = "NormalsPrepass",
                     .colors = {ColorTargetUse{.target = Normals}},
                     .depth = DepthTargetUse{.target = SceneDepth},
                     .kind =
                         MeshPassDesc{
                             .state =
                                 MeshPassState{.fragmentShader = normalsShader}
                         }
                 },
                 PassDesc{
                     .name = "Opaque",
                     .colors = {ColorTargetUse{.target = SceneColor}},
                     .depth =
                         DepthTargetUse{
                             .target = SceneDepth,
                             .load = RHILoadAction::Load
                         },
                     .kind =
                         MeshPassDesc{
                             .state =
                                 MeshPassState{
                                     .depthFunc = RHIComparisonFunc::Equal,
                                     .depthWrite = false
                                 }
                         }
                 },
                 PassDesc{
                     .name = "Outline",
                     .colors = {ColorTargetUse{
                         .target = SceneColor,
                         .load = RHILoadAction::Load
                     }},
                     .reads = {Normals, SceneDepth},
                     .kind =
                         FullscreenPassDesc{
                             .fragmentShader = outlineShader,
                             .blend = OutlineBlend(),
                             .params = {0.1f, 0.5f, 0.05f, 100.0f}
                         }
                 },
                 PassDesc{
                     .name = "Translucent",
                     .colors = {ColorTargetUse{
                         .target = SceneColor,
                         .load = RHILoadAction::Load
                     }},
                     .depth =
                         DepthTargetUse{
                             .target = SceneDepth,
                             .load = RHILoadAction::Load,
                             .store = RHIStoreAction::DontCare
                         },
                     .kind =
                         MeshPassDesc{
                             .filter =
                                 DrawFilter{
                                     .domains = MaterialDomain::Translucent
                                 },
                             .order = DrawOrder::FarFirst,
                             .state =
                                 MeshPassState{
                                     .depthFunc = RHIComparisonFunc::Less,
                                     .depthWrite = false
                                 }
                         }
                 }},
            .sceneColor = SceneColor
        };
        // both in display values, so the chain adds one intermediate,
        // target 4
        const std::array post{
            Post("Post1", "fs_post1"),
            Post("Post2", "fs_post2")
        };
        appendPostChain(desc, SceneColor, post);

        return desc;
    }

    void AddPrimitive(RenderScene& scene, MaterialDomain domain) {
        const auto material = scene.Materials().Add(
            MaterialResource{
                .pipeline = MaterialPipelineDesc{
                    .vertexShader =
                        {.path = "Engine/Shader/X.slang",
                         .entryPoint = "vs_main"},
                    .fragmentShader =
                        {.path = "Engine/Shader/X.slang",
                         .entryPoint = "fs_main"},
                    .domain = domain,
                    .profile = "sm_6_8"
                }
            }
        );
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
                .mesh = mesh
            }
        );
    }

    class Fixture {
    public:
        FakeDevice device;
        RenderScene scene;
        SceneRenderer renderer{device, SceneRendererDesc{}};
        FakeCommandList cmdList;
        FakeTexture backBuffer{BackBufferFormat, Width, Height, 0xBB};
        FakeBuffer indexBuffer{64};
        FramePipeline
            pipeline{device, ToonShaped(), BackBufferFormat, Width, Height};

        Fixture() {
            AddPrimitive(scene, MaterialDomain::Opaque);
            AddPrimitive(scene, MaterialDomain::Translucent);
        }

        void Frame() {
            FrameInputs inputs{
                .backBuffer = &backBuffer,
                .indices = RHIIndexBufferView{.buffer = &indexBuffer}
            };
            renderer.BeginFrame(scene);
            pipeline.Prepare(renderer, scene, {});
            renderer.Upload();
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
    };
}

TEST(ComposedPipeline, AToonShapedPipelineComposesFromPublicHeaders) {
    using enum RHIResourceUsage;

    Fixture f;

    for(int frame = 0; frame < 2; ++frame) {
        f.Frame();

        ASSERT_EQ(f.cmdList.passes.size(), 6u);
        auto& depth = *f.Pass("NormalsPrepass").depth->texture;
        auto& normals = *f.Pass("NormalsPrepass").colors[0].texture;
        auto& color = *f.Pass("Opaque").colors[0].texture;
        auto& intermediate = *f.Pass("Post1").colors[0].texture;
        auto& back = f.backBuffer;

        EXPECT_EQ(
            f.Pass("NormalsPrepass").acquires,
            (TextureBarriers{
                MakeCrossSubmissionBarrier(depth, DepthWrite, DepthWrite, true),
                MakeCrossSubmissionBarrier(
                    normals,
                    SampledFragment,
                    RenderTarget,
                    true
                )
            })
        );
        EXPECT_EQ(
            f.Pass("NormalsPrepass").releases,
            (TextureBarriers{
                MakeBarrier(depth, DepthWrite, DepthWrite),
                MakeBarrier(normals, RenderTarget, SampledFragment)
            })
        );
        EXPECT_EQ(
            f.Pass("Opaque").acquires,
            (TextureBarriers{
                MakeBarrier(depth, DepthWrite, DepthWrite),
                MakeCrossSubmissionBarrier(
                    color,
                    SampledFragment,
                    RenderTarget,
                    true
                )
            })
        );
        EXPECT_EQ(
            f.Pass("Opaque").releases,
            (TextureBarriers{
                MakeBarrier(depth, DepthWrite, SampledFragment),
                MakeBarrier(color, RenderTarget, RenderTarget)
            })
        );
        EXPECT_EQ(
            f.Pass("Outline").acquires,
            (TextureBarriers{
                MakeBarrier(depth, DepthWrite, SampledFragment),
                MakeBarrier(normals, RenderTarget, SampledFragment),
                MakeBarrier(color, RenderTarget, RenderTarget)
            })
        );
        EXPECT_EQ(
            f.Pass("Outline").releases,
            (TextureBarriers{
                MakeBarrier(depth, SampledFragment, DepthWrite),
                MakeBarrier(color, RenderTarget, RenderTarget)
            })
        );
        EXPECT_EQ(
            f.Pass("Translucent").acquires,
            (TextureBarriers{
                MakeBarrier(depth, SampledFragment, DepthWrite),
                MakeBarrier(color, RenderTarget, RenderTarget)
            })
        );
        EXPECT_EQ(
            f.Pass("Translucent").releases,
            TextureBarriers{MakeBarrier(color, RenderTarget, SampledFragment)}
        );
        EXPECT_EQ(
            f.Pass("Post1").acquires,
            (TextureBarriers{
                MakeBarrier(color, RenderTarget, SampledFragment),
                MakeCrossSubmissionBarrier(
                    intermediate,
                    SampledFragment,
                    RenderTarget,
                    true
                )
            })
        );
        EXPECT_EQ(
            f.Pass("Post1").releases,
            TextureBarriers{
                MakeBarrier(intermediate, RenderTarget, SampledFragment)
            }
        );
        EXPECT_EQ(
            f.Pass("Post2").acquires,
            (TextureBarriers{
                MakeBarrier(back, Undefined, RenderTarget),
                MakeBarrier(intermediate, RenderTarget, SampledFragment)
            })
        );
        EXPECT_EQ(
            f.Pass("Post2").releases,
            TextureBarriers{MakeBarrier(back, RenderTarget, Present)}
        );

        EXPECT_TRUE(f.cmdList.violations.empty())
            << f.cmdList.violations.front().what;
        EXPECT_TRUE(f.cmdList.unconsumedAtClose.empty());
#if CROWY_FRAME_STATS
        EXPECT_EQ(f.cmdList.GetStats().renderPassCount, 6u);
        EXPECT_EQ(f.cmdList.GetStats().barrierEdgeCount, 22u);
#endif
    }

    // a depth target read is a shader resource too; every other target is
    // attached and read
    const auto& creates = f.device.textureCreates;
    ASSERT_EQ(creates.size(), 4u);
    EXPECT_EQ(
        creates[0].usage,
        combine(RHITextureUsage::DepthStencil, RHITextureUsage::ShaderResource)
    );
    for(usize i = 1; i < creates.size(); ++i) {
        EXPECT_EQ(
            creates[i].usage,
            combine(
                RHITextureUsage::RenderTarget,
                RHITextureUsage::ShaderResource
            )
        ) << i;
    }
    EXPECT_EQ(creates[1].clearColor, (Color{0.5f, 0.5f, 0.5f, 0.0f}));

    f.pipeline.Resize(128, 96);
    ASSERT_EQ(creates.size(), 8u);
    for(usize i = 4; i < creates.size(); ++i) {
        EXPECT_EQ(creates[i].width, 128u) << i;
        EXPECT_EQ(creates[i].height, 96u) << i;
    }
    EXPECT_EQ(f.device.deferred.size(), 4u);

    f.Frame();
    EXPECT_TRUE(f.cmdList.violations.empty());
}

TEST(ComposedPipeline, AToonShapedPipelineKeysItsOwnStates) {
    Fixture f;
    f.Frame();

    const auto& creates = f.device.pipelineCreates;
    const auto normalsKey = std::ranges::find_if(
        creates,
        [](const RHIGraphicsPipelineStateDesc& d) {
            return d.fragmentShader == normalsShader;
        }
    );
    ASSERT_NE(normalsKey, creates.end());
    // the fragment override writes the one normals target
    ASSERT_EQ(normalsKey->renderTargetCount, 1u);
    EXPECT_EQ(normalsKey->renderTargetFormats[0], RHIPixelFormat::RGBA8_UNORM);
    ASSERT_TRUE(normalsKey->depthStencil.has_value());
    EXPECT_EQ(normalsKey->depthStencil->depthFunc, RHIComparisonFunc::Less);
    EXPECT_TRUE(normalsKey->depthStencil->depthWriteEnable);

    const auto outlineKey = std::ranges::find_if(
        creates,
        [](const RHIGraphicsPipelineStateDesc& d) {
            return d.fragmentShader == outlineShader;
        }
    );
    ASSERT_NE(outlineKey, creates.end());
    EXPECT_EQ(outlineKey->blend, OutlineBlend());
    EXPECT_FALSE(outlineKey->depthStencil.has_value());
    ASSERT_EQ(outlineKey->renderTargetCount, 1u);
    EXPECT_EQ(outlineKey->renderTargetFormats[0], RHIPixelFormat::RGBA16_FLOAT);

    // the UI rides a post pass with no depth
    EXPECT_EQ(
        f.pipeline.Overlay(),
        (OverlayFormats{
            .color = BackBufferFormat,
            .depth = RHIPixelFormat::Unknown
        })
    );
    EXPECT_TRUE(f.cmdList.violations.empty());
}
