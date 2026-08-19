#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>
#include "AppFramework.hpp"
#include "InputProvider.hpp"
#include "Kepler.hpp"
#include "LinearAlgebra.hpp"
#include "OrbitTrail.hpp"
#include "RHIBuffer.hpp"
#include "RHIPipelineState.hpp"
#include "UIRenderer.hpp"

#include <imgui.h>

namespace Crowy
{
    struct UIContext{
        // heliocentricity: 0 geocentric, 1 heliocentric. The point of the demo.
        f32 alpha = 0.0f;

        // read-only stats
        f32 orbitTurns = 1.0f;
        f32 halfHeight = 1.0f;
        f64 frameTimeMs = 0.0;
        u32 filled = 0;
        u64 totalTicks = 0;
    };

    // Planet trails drawn as screen-space expanded polylines, straight out of
    // the GPU ring buffer. One draw per body, one instance per segment.
    //
    // The origin is interpolated between Earth and the Sun rather than the
    // camera being moved: the slider is a shader constant, not a view matrix.
    // Push it to 0 and the heliocentric ellipses come apart into Ptolemy's
    // epicycles - Mars doubling back on itself, Venus tracing a rose.
    //
    // Radius is compressed logarithmically after the interpolation, so the
    // warp centre follows whatever the current origin is. 30 AU against
    // 0.39 AU is 77:1 linear and 10:1 through the log.
    //
    // Zoom and trail length are keys because the input layer has no scroll
    // wheel; the panel takes them over later.
    //   Up / Down     zoom
    //   Left / Right  trail length (orbit turns)
    //   Space         pause
    class OrbitFrameSample: public App{
        using App::App;

        static constexpr f64 DAY_PER_SAMPLE = 1.0;
        static constexpr f64 START_DAY = 0.0;
        static constexpr f64 TIME_SCALE = 120.0;   // sim days per second

        static constexpr Color BACKGROUND{0.02f, 0.02f, 0.05f, 1.0f};

        // half-heights are in warped units now: the whole system reaches
        // log(1 + 30) = 3.43, and Mercury sits at log(1 + 0.39) = 0.33
        static constexpr f32 ZOOM_LOG_MIN = -2.0f;   // ~0.14
        static constexpr f32 ZOOM_LOG_MAX = 2.1f;    // ~8.2
        static constexpr f32 ZOOM_SPEED = 1.2f;      // log units per second

        // The plan says 3.0, but Venus's rose only closes after 13 Venus years
        // (8 Earth years), and that is the shape Step 4 asks to see. Anything
        // that runs past the ring clamps to what is stored, so the ceiling
        // costs nothing.
        static constexpr f32 TURNS_MIN = 0.05f, TURNS_MAX = 15.0f;
        static constexpr f32 TURNS_SPEED = 2.5f;     // per second

        // the camera sits off the ecliptic looking back down +Z; nothing in the
        // scene reaches anywhere near these planes
        static constexpr f32 CAMERA_DISTANCE = 100.0f;
        static constexpr f32 NEAR_Z = 1.0f, FAR_Z = 200.0f;

        // mirrors BodyDraw in Engine/Shader/OrbitTrail.slang
        struct OrbitBodyDraw{
            f32 colorR, colorG, colorB;
            u32 segCount;
        };
        static_assert(sizeof(OrbitBodyDraw) == 16);
        static_assert(offsetof(OrbitBodyDraw, segCount) == 12);

        // mirrors TrailPush in Engine/Shader/OrbitTrail.slang
        struct TrailPush{
            u64 samples;
            u64 bodies;
            f32 viewportX, viewportY;
            f32 thicknessPx;
            f32 alpha;
            u32 head;
            u32 capacity;
        };
        static_assert(sizeof(TrailPush) == 40);
        static_assert(offsetof(TrailPush, bodies) == 8);
        static_assert(offsetof(TrailPush, viewportX) == 16);
        static_assert(offsetof(TrailPush, thicknessPx) == 24);
        static_assert(offsetof(TrailPush, alpha) == 28);
        static_assert(offsetof(TrailPush, head) == 32);
        static_assert(offsetof(TrailPush, capacity) == 36);

        struct FrameUniforms{
            Mat4 viewProj;
        };

        // the trail is pulled by index, so the ring's Vec3 packing is the
        // shader's StructuredBuffer<float3> stride
        static_assert(sizeof(Vec3) == 12);

        // warm hues inward, cold outward, and the Sun deliberately not yellow -
        // at alpha 0 it takes over Earth's ring and the two want telling apart
        static constexpr std::array<Vec3, ORBIT_BODY_COUNT> BODY_COLORS{
            Vec3{1.00f, 0.95f, 0.70f},  // Sun
            Vec3{0.78f, 0.72f, 0.66f},  // Mercury
            Vec3{0.95f, 0.72f, 0.40f},  // Venus
            Vec3{0.40f, 0.72f, 1.00f},  // Earth
            Vec3{0.94f, 0.42f, 0.32f},  // Mars
            Vec3{0.90f, 0.76f, 0.55f},  // Jupiter
            Vec3{0.85f, 0.80f, 0.55f},  // Saturn
            Vec3{0.60f, 0.88f, 0.90f},  // Uranus
            Vec3{0.42f, 0.55f, 0.95f}   // Neptune
        };

        RHIDevice* device = nullptr;
        RAII<OrbitTrail> trail;

        RHIGraphicsPipelineStateRAII pso;
        RHIBufferRAII frameCB;
        RHIBufferRAII bodyBuffer;

        RAII<UIRenderer> uiRenderer;
        UIContext context;
        Widget panel;

        std::array<OrbitBodyDraw, ORBIT_BODY_COUNT> bodyDraws{};

        f64 pendingSeconds = 0.0;
        bool paused = false;

        // 3.8 in warped units clears log(1 + 30) = 3.43 with a margin
        f32 zoomLog = 1.34f;
        f32 orbitTurns = 1.0f;
        f32 thicknessPx = 2.0f;

        f32 viewportWidth = 1.0f, viewportHeight = 1.0f;

        // held between ProcessInput and OnUpdate
        f32 zoomInput = 0.0f, turnsInput = 0.0f;

        void OnInit(RHIDevice& device, RHISwapchain& swapchain) override{
            this->device = &device;

            trail = std::make_unique<OrbitTrail>(
                device,
                DAY_PER_SAMPLE,
                ORBIT_TRAIL_CAPACITY,
                RHIResourceUsage::SampledVertex
            );
            // without this the outer planets would take real minutes to draw
            // themselves in - Neptune's orbit is 60,225 ticks
            trail->Prefill(START_DAY);

            pso = device.CreatePipelineState(RHIGraphicsPipelineStateDesc{
                .preRasterizer = RHILegacyFrontendDesc{
                    .topology = RHIPrimitiveTopology::TriangleStrip,
                    .vertexShader = {
                        .path = "Engine/Shader/OrbitTrail.slang",
                        .entryPoint = "vs_main"
                    }
                },
                .rasterizer = RHIRasterizerState{
                    // an expanded quad's winding follows the segment direction,
                    // so half of every trail would vanish under back-face culling
                    .cullMode = RHICullMode::None
                },
                .fragmentShader = {
                    .path = "Engine/Shader/OrbitTrail.slang",
                    .entryPoint = "fs_main"
                },
                .blend = RHIBlendState{
                    .renderTargets = {
                        RHIRenderTargetBlendState{
                            .blendEnable = true,
                            .srcBlend = RHIBlend::SrcAlpha,
                            .dstBlend = RHIBlend::InvSrcAlpha,
                            .blendOp = RHIBlendOp::Add,
                            .srcBlendAlpha = RHIBlend::One,
                            .dstBlendAlpha = RHIBlend::InvSrcAlpha,
                            .blendOpAlpha = RHIBlendOp::Add
                        }
                    }
                },
                .renderTargetFormats = {
                    swapchain.GetFormat()
                },
                .renderTargetCount = 1,
                // SV_StartInstanceLocation needs it
                .profile = "sm_6_8"
            }, "OrbitTrailPSO");

            frameCB = device.CreateBuffer(RHIBufferCreateDesc{
                .size = sizeof(FrameUniforms),
                .usage = RHIBufferUsage::ConstantBuffer,
                .access = RHIMemoryAccess::CPUWrite
            }, "OrbitFrameCB");

            bodyBuffer = device.CreateBuffer(RHIBufferCreateDesc{
                .size = sizeof(bodyDraws),
                .usage = RHIBufferUsage::ShaderResource,
                .access = RHIMemoryAccess::CPUWrite
            }, "OrbitBodyDraws");

            for(u32 b=0; b<ORBIT_BODY_COUNT; ++b){
                bodyDraws[b] = OrbitBodyDraw{
                    .colorR = BODY_COLORS[b].x,
                    .colorG = BODY_COLORS[b].y,
                    .colorB = BODY_COLORS[b].z
                };
            }

            uiRenderer = std::make_unique<UIRenderer>(
                device,
                swapchain.GetFormat()
            );
            BuildPanel();
        }

        void BuildPanel(){
            // the getters capture the context by reference and outlive this
            // call - it is a member, so that holds
            panel = Column({
                Text{.data = "Heliocentricity"},
                Slider{
                    .label = "alpha",
                    .onChanged = [](UIContext& c, f32 v){
                        c.alpha = v;
                    },
                    .v = context.alpha,
                    .v_min = 0.0f,
                    .v_max = 1.0f
                },
                Text{.data = "0 geocentric  <->  1 heliocentric"},
                Text{.data = ""},
                Text{.data = "keys: Up/Down zoom, Left/Right trail, Space pause"},
                FloatField{
                    .label = "orbit turns",
                    .get = [&ctx = context]{ return ctx.orbitTurns; }
                },
                FloatField{
                    .label = "half-height (warped)",
                    .get = [&ctx = context]{ return ctx.halfHeight; }
                },
                IntField{
                    .label = "samples stored",
                    .get = [&ctx = context]{
                        return static_cast<int>(ctx.filled);
                    }
                },
                IntField{
                    .label = "ticks pushed",
                    .get = [&ctx = context]{
                        return static_cast<int>(ctx.totalTicks);
                    }
                },
                FloatField{
                    .label = "ms / frame",
                    .get = [&ctx = context]{
                        return static_cast<f32>(ctx.frameTimeMs);
                    }
                }
            });
        }

        void ProcessInput(const InputProvider& input) override{
            // the OS layer already drops events ImGui claimed, but IsKeyDown is
            // a level query: a key-up swallowed mid-drag would stick
            if(ImGui::GetIO().WantCaptureKeyboard){
                zoomInput = 0.0f;
                turnsInput = 0.0f;

                return;
            }

            zoomInput =
                (input.IsKeyDown(KeyCode::Up) ? 1.0f : 0.0f) -
                (input.IsKeyDown(KeyCode::Down) ? 1.0f : 0.0f);
            turnsInput =
                (input.IsKeyDown(KeyCode::Right) ? 1.0f : 0.0f) -
                (input.IsKeyDown(KeyCode::Left) ? 1.0f : 0.0f);

            if(input.IsKeyPressed(KeyCode::Space))
                paused = !paused;
        }

        void OnUpdate(f64 deltaTime, f64) override{
            // the sim step is consumed in OnRecord, where the copy that pairs
            // with it is recorded - OrbitTrail wants exactly one Advance per
            // Record and this is the only way to guarantee it
            pendingSeconds += deltaTime;

            const auto dt = static_cast<f32>(deltaTime);
            // zoom out means a larger half-height, so Up shrinks it
            zoomLog = std::clamp(
                zoomLog - zoomInput * ZOOM_SPEED * dt,
                ZOOM_LOG_MIN,
                ZOOM_LOG_MAX
            );
            orbitTurns = std::clamp(
                orbitTurns + turnsInput * TURNS_SPEED * dt,
                TURNS_MIN,
                TURNS_MAX
            );

            context.frameTimeMs = 1000.0 * deltaTime;
            context.orbitTurns = orbitTurns;
            context.halfHeight = std::exp(zoomLog);
        }

        Mat4 ViewProj() const{
            const auto halfHeight = std::exp(zoomLog);
            const auto aspect = viewportWidth / viewportHeight;

            const auto proj = orthographic(
                2.0f * halfHeight * aspect,
                2.0f * halfHeight,
                NEAR_Z,
                FAR_Z
            );
            // straight down the ecliptic normal, no rotation: +X right,
            // +Y up, and the ecliptic is the screen
            const auto view = viewMat(
                Vec3{0.0f, 0.0f, -CAMERA_DISTANCE},
                Vec4{0.0f, 0.0f, 0.0f, 1.0f}
            );

            return proj * view;
        }

        void UpdateBodyDraws(){
            const auto filled = trail->Filled();

            for(u32 b=0; b<ORBIT_BODY_COUNT; ++b){
                // each body shows the same fraction of its own orbit; a shared
                // length would make Neptune a stub and Mercury a smear
                const auto wanted = static_cast<f64>(orbitTurns) *
                    TrailPeriodDays(b) / DAY_PER_SAMPLE;
                const auto usable = std::min(
                    filled,
                    static_cast<u32>(std::min(wanted, static_cast<f64>(filled)))
                );

                // n samples make n - 1 segments, and that is also what keeps
                // the oldest segment from reaching across the ring seam
                bodyDraws[b].segCount = usable > 1 ? usable - 1 : 0;
            }

            bodyBuffer->Upload(bodyDraws.data(), sizeof(bodyDraws));
        }

        void OnRecord(RHICommandList& cmdList, const RHIColorAttachment& backBuffer) override{
            if(!paused){
                trail->Advance(std::exchange(pendingSeconds, 0.0), TIME_SCALE);
            }
            else{
                pendingSeconds = 0.0;
            }

            // read off the backbuffer rather than tracked through OnResize:
            // that event only fires on an actual resize, never at startup
            viewportWidth = static_cast<f32>(backBuffer.texture->GetWidth());
            viewportHeight = static_cast<f32>(backBuffer.texture->GetHeight());

            context.filled = trail->Filled();
            context.totalTicks = trail->Stats().totalTicks;

            UpdateBodyDraws();
            frameCB->Upload(FrameUniforms{.viewProj = ViewProj()});

            // outside any pass: the copies are their own blit pass
            const auto trailEdge = trail->Record(cmdList);

            // the slider callbacks run in here, so alpha below is this frame's
            uiRenderer->Prepare(cmdList, panel, context);

            auto colorAttachment = backBuffer;
            colorAttachment.clearColor = BACKGROUND;
            std::array colorAttachments = {colorAttachment};

            std::vector<RHITextureBarrier> textureAcquires{
                AcquireBackBuffer(backBuffer)
            };
            // the pass samples the font atlas Prepare just refreshed
            textureAcquires.append_range(uiRenderer->TextureAcquires());

            // only when something was actually copied; an idle frame leaves the
            // ring already resting where the vertex stage wants it
            std::vector<RHIBufferBarrier> bufferAcquires;
            if(trailEdge.has_value())
                bufferAcquires.push_back(*trailEdge);

            cmdList.BeginRenderPass(RHIRenderPassDesc{
                .colorAttachments = colorAttachments
            }, textureAcquires, bufferAcquires);
            cmdList.SetViewport(FullViewport(*backBuffer.texture));
            cmdList.SetScissorRect(FullScissorRect(*backBuffer.texture));

            cmdList.SetPipelineState(*pso);
            cmdList.SetGraphicsConstantBuffer(*frameCB, 0);
            cmdList.SetPushGraphicsConstants(TrailPush{
                .samples = trail->Buffer().GetReadableID(
                    static_cast<u32>(sizeof(Vec3))
                ),
                .bodies = bodyBuffer->GetReadableID(
                    static_cast<u32>(sizeof(OrbitBodyDraw))
                ),
                .viewportX = viewportWidth,
                .viewportY = viewportHeight,
                .thicknessPx = thicknessPx,
                .alpha = context.alpha,
                .head = trail->Head(),
                .capacity = trail->Capacity()
            });

            for(u32 b=0; b<ORBIT_BODY_COUNT; ++b){
                if(bodyDraws[b].segCount == 0)
                    continue;

                // baseInstance is the drawID the shader reads as bodyIdx;
                // Step 8 turns this loop into nine RHIDrawArgs and one
                // ExecuteIndirect without the shader noticing
                cmdList.Draw(4, bodyDraws[b].segCount, 0, b);
            }

            uiRenderer->Record(cmdList);

            const std::array releases{ReleaseBackBuffer(backBuffer)};
            cmdList.EndRenderPass(releases);
        }
    };
}

int main(void){
    using namespace Crowy;

    const WindowConfig windowConfig{
        .title = "OrbitFrameSample",
        .width = 1280, .height = 720,
        .format = RHIPixelFormat::RGBA8_UNORM,
        .fullscreen = false,
        .resizable = true,
    };
    return Main<OrbitFrameSample>(windowConfig);
}
